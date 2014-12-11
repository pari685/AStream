#!/usr/local/bin/python
"""
Author:            Parikshit Juluri
Contact:           pjuluri@umkc.edu
Testing:
    import dash_client
    mpd_file = <MPD_FILE>
    dash_client.playback_duration(mpd_file, 'http://198.248.242.16:8005/')

    From commandline:
    python dash_client.py -m "http://198.248.242.16:8006/media/mpd/x4ukwHdACDw.mpd" -p "all"
    python dash_client.py -m "http://127.0.0.1:8000/media/mpd/x4ukwHdACDw.mpd" -p "basic"

"""
from __future__ import division
import read_mpd
import urlparse
import urllib2
import random
import os
import sys
import errno
import timeit
import httplib
from string import ascii_letters, digits
from argparse import ArgumentParser
from multiprocessing import Process, Queue
from collections import defaultdict
from adaptation import basic_dash, weighted_dash, WeightedMean
import config_dash
import dash_buffer
from configure_log_file import configure_log_file
import time

try:
    WindowsError
except NameError:
    from shutil import WindowsError


# Constants
DEFAULT_PLAYBACK = 'BASIC'
DOWNLOAD_CHUNK = 1024

# Globals for arg parser with the default values
# Not sure if this is the correct way ....
MPD = None
LIST = False
PLAYBACK = DEFAULT_PLAYBACK
DOWNLOAD = False


class DashPlayback:
    """
    Audio[bandwidth] : {duration, url_list}
    Video[bandwidth] : {duration, url_list}
    """
    def __init__(self):

        self.min_buffer_time = None
        self.playback_duration = None
        self.audio = dict()
        self.video = dict()


def get_mpd(url):
    """ Module to download the MPD from the URL and save it to file"""
    try:
        connection = urllib2.urlopen(url, timeout=10)
    except urllib2.HTTPError, error:
        config_dash.LOG.error("Unable to download MPD file HTTP Error: %s" % error.code)
        return None
    except urllib2.URLError:
        error_message = "URLError. Unable to reach Server.Check if Server active"
        config_dash.LOG.error(error_message)
        print error_message
        return None
    except IOError, httplib.HTTPException:
        message = "Unable to , file_identifierdownload MPD file HTTP Error."
        config_dash.LOG.error(message)
        return None
    
    mpd_data = connection.read()
    connection.close()
    mpd_file = url.split('/')[-1]
    mpd_file_handle = open(mpd_file, 'w')
    mpd_file_handle.write(mpd_data)
    mpd_file_handle.close()
    config_dash.LOG.info("DOwnloaded the MPD file {}".format(mpd_file))
    return mpd_file


def get_bandwidth(data, duration):
    """ Module to determine the bandwidth for a segment
    download"""
    return data * 8/duration


def get_domain_name(url):
    """ Module to obtain the domain name from the URL
        From : http://stackoverflow.com/questions/9626535/get-domain-name-from-url
    """
    parsed_uri = urlparse.urlparse(url)
    domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_uri)
    return domain


def id_generator(id_size=6):
    """ Module to create a random string with uppercase 
        and digits.
    """
    return 'TEMP_' + ''.join(random.choice(ascii_letters+digits) for _ in range(id_size))


def download_segment(segment_url, dash_folder):
    """ Module to download the segment """
    try:
        connection = urllib2.urlopen(segment_url)
    except urllib2.HTTPError, error:
        config_dash.LOG.error("Unable to download DASH Segment {} HTTP Error:{} ".format(segment_url, str(error.code)))
        return None
    parsed_uri = urlparse.urlparse(segment_url)
    segment_path = '{uri.path}'.format(uri=parsed_uri)
    while segment_path.startswith('/'):
        segment_path = segment_path[1:]        
    segment_filename = os.path.join(dash_folder, os.path.basename(segment_path))
    make_sure_path_exists(os.path.dirname(segment_filename))
    segment_file_handle = open(segment_filename, 'wb')
    segment_size = 0
    while True:
        segment_data = connection.read(DOWNLOAD_CHUNK)
        segment_size += len(segment_data)
        segment_file_handle.write(segment_data)
        if len(segment_data) < DOWNLOAD_CHUNK:
            break
    connection.close()
    segment_file_handle.close()
    return segment_size, segment_filename


def get_media_all(domain, media_info, file_identifier, done_queue):
    """ Download the media from the list of URL's in media
    """
    bandwidth, media_dict = media_info
    media = media_dict[bandwidth]
    media_start_time = timeit.default_timer()
    for segment in [media.initialization] + media.url_list:
        start_time = timeit.default_timer()
        segment_url = urlparse.urljoin(domain, segment)
        _, segment_file = download_segment(segment_url, file_identifier)
        elapsed = timeit.default_timer() - start_time
        if segment_file:
            done_queue.put((bandwidth, segment_url, elapsed))
    media_download_time = timeit.default_timer() - media_start_time
    done_queue.put((bandwidth, 'STOP', media_download_time))
    return None


def make_sure_path_exists(path):
    """ Module to make sure the path exists if not create it
    """
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise


def print_representations(dp_object):
    """ Module to print the representations"""
    print "The DASH media has the following video representations/bitrates"
    for bandwidth in dp_object.video:
        print bandwidth


def start_playback_smart(dp_object, domain, playback_type=None, download=False, video_segment_duration=None):
    """ Module that downloads the MPD-FIle and download
        all the representations of the Module to download
        the MPEG-DASH media.
    """
    # Initialize the DASH buffer
    dash_player = dash_buffer.DashPlayer(dp_object.playback_duration, video_segment_duration)
    dash_player.start()
    # A folder to save the segments in
    file_identifier = id_generator()
    config_dash.LOG.info("The segments are stored in %s" % file_identifier)
    dp_list = defaultdict(defaultdict)
    # Creating a Dictionary of all that has the URLs for each segment and different bitrates
    for bitrate in dp_object.video:
        # Getting the URL list for each bitrate
        dp_object.video[bitrate] = read_mpd.get_url_list(dp_object.video[bitrate], video_segment_duration,
                                                         dp_object.playback_duration, bitrate)
        config_dash.LOG.debug("Playback Duration = {}".format(video_segment_duration))
        if "$Bandwidth$" in dp_object.video[bitrate].initialization:
            dp_object.video[bitrate].initialization = dp_object.video[bitrate].initialization.replace(
                "$Bandwidth$", str(bitrate))
        media_urls = [dp_object.video[bitrate].initialization] + dp_object.video[bitrate].url_list
        for segment_count, segment_url in enumerate(media_urls):
            # segment_duration = dp_object.video[bitrate].segment_duration
            dp_list[segment_count][bitrate] = segment_url
    bitrates = dp_object.video.keys()
    bitrates.sort()
    current_bitrate = None
    average_dwn_time = 0
    segment_download_time = 0
    segment_files = []
    weighted_mean_object = None
    for segment_number, segment in enumerate(dp_list):
        config_dash.LOG.debug("Processing the segment {}".format(segment_number))
        if segment_number == 0:
            current_bitrate = bitrates[0]
        else:
            if playback_type.upper() == "BASIC":
                current_bitrate, average_dwn_time = basic_dash(segment_number, bitrates, average_dwn_time,
                                                               segment_download_time, current_bitrate)
                config_dash.LOG.info("Basic-DASH: Selected {} for the segment {}".format(current_bitrate,
                                                                                         segment_number + 1))
            elif playback_type.upper() == "SMART":
                if not weighted_mean_object:
                    weighted_mean_object = WeightedMean()
                    config_dash.LOG.debug("Initializing the weighted Mean object")
                if segment_number < len(dp_list)-1:
                    current_bitrate, delay = weighted_dash(bitrates, dash_player,
                                                           weighted_mean_object.weighted_mean_rate,
                                                           current_bitrate,
                                                           get_segment_sizes(dp_object, segment_number+1))
            else:
                config_dash.LOG.error("Unknown playback type: {}".format(playback_type))
        segment_path = dp_list[segment][current_bitrate]
        segment_url = urlparse.urljoin(domain, segment_path)
        start_time = timeit.default_timer()
        try:
            segment_size, segment_filename = download_segment(segment_url, file_identifier)
        except IOError, e:
            config_dash.LOG.error("Unable to save segement %s" % e)
            return None
        segment_download_time = timeit.default_timer() - start_time
        if playback_type.upper() == "SMART" and weighted_mean_object:
            weighted_mean_object.update_weighted_mean(segment_size, segment_download_time)

        segment_info = {'playback_length': video_segment_duration,
                        'size': segment_size,
                        'bitrate': current_bitrate,
                        'data': segment_filename,
                        'URI': segment_url,
                        'segment_number': segment_number}
        dash_player.write(segment_info)
        segment_files.append(segment_filename)
        config_dash.LOG.info("Downloaded %s. Size = %s in %s seconds" % (
            segment_url, segment_size, str(segment_download_time)))
    while dash_player.playback_state not in dash_buffer.EXIT_STATES:
        time.sleep(1)
    if not download:
        clean_files(file_identifier)


def get_segment_sizes(dp_object, segment_number):
    """ Module to get the segment sizes for the segment_number
    :param dp_object:
    :param segment_number:
    :return:
    """
    segment_sizes = dict([(bitrate, dp_object.video[bitrate].segment_sizes[segment_number]) for bitrate in dp_object.video])
    config_dash.LOG.debug("The segment sizes of {} are {}".format(segment_number, segment_sizes))
    return segment_sizes


def clean_files(folder_path):
    """
    :param folder_path: Folder to be deleted
    """
    if os.path.exists(folder_path):
        try:
            os.rmdir(folder_path)
            config_dash.LOG.info("Deleting the folder {}".format(folder_path))
        except (WindowsError, OSError), e:
            config_dash.LOG.info("Unable to delete the folder {}. {}".format(folder_path, e))


def start_playback_all(dp_object, domain):
    """ Module that downloads the MPD-FIle and download all the representations of 
        the Module to download the MPEG-DASH media.
    """
    audio_done_queue = Queue()
    video_done_queue = Queue()
    processes = []
    file_identifier = id_generator(6)
    config_dash.LOG.info("File Segements are in %s" % file_identifier)
    for bitrate in dp_object.audio:
        # Get the list of URL's (relative location) for the audio 
        dp_object.audio[bitrate] = read_mpd.get_url_list(bitrate, dp_object.audio[bitrate],
                                                         dp_object.playback_duration)
        # Create a new process to download the audio stream.
        # The domain + URL from the above list gives the 
        # complete path
        # The fil-identifier is a random string used to 
        # create  a temporary folder for current session
        # Audio-done queue is used to exchange information
        # between the process and the calling function.
        # 'STOP' is added to the queue to indicate the end 
        # of the download of the sesson
        process = Process(target=get_media_all, args=(domain, (bitrate, dp_object.audio),
                                                      file_identifier, audio_done_queue))
        process.start()
        processes.append(process)

    for bitrate in dp_object.video:
        dp_object.video[bitrate] = read_mpd.get_url_list(bitrate, dp_object.video[bitrate],
                                                         dp_object.playback_duration)
        # Same as download audio
        process = Process(target=get_media_all, args=(domain, (bitrate, dp_object.video),
                                                      file_identifier, video_done_queue))
        process.start()
        processes.append(process)
    for process in processes:
        process.join()
    count = 0
    for queue_values in iter(video_done_queue.get, None):
        bitrate, status, elapsed = queue_values
        if status == 'STOP':
            config_dash.LOG.critical("Completed download of %s in %f " % (bitrate, elapsed))
            count += 1
            if count == len(dp_object.video):
                # If the download of all the videos is done the stop the
                config_dash.LOG.critical("Finished download of all video segments")
                break


def create_arguments(parser):
    """ Adding arguments to the parser """
    parser.add_argument('-m', '--MPD',                   
                        help="Url to the MPD File")
    parser.add_argument('-l', '--LIST', action='store_true',
                        help="List all the representations")
    parser.add_argument('-p', '--PLAYBACK',
                        default=DEFAULT_PLAYBACK,
                        help="Playback type (basic, smart, or all)")
    parser.add_argument('-d', '--DOWNLOAD', action='store_true',
                        default=False,
                        help="Keep the video files after playback")


def main():
    """ Main Program wrapper"""
    # configure the log file
    configure_log_file()
    # Create arguments
    parser = ArgumentParser(description='Process Client paameters')
    create_arguments(parser)
    args = parser.parse_args()
    globals().update(vars(args))
    if not MPD:
        # config_dash.LOG.error('Downloading MPD file %s' % MPD)
        print "ERROR: Please provide the URL to the MPD file. Try Again.."
        return None
    config_dash.LOG.info('Downloading MPD file %s' % MPD)
    
    # Retrieve the MPD files for the video
    mpd_file = get_mpd(MPD)
    domain = get_domain_name(MPD)
    dp_object = DashPlayback()
    # Reading the MPD file created
    dp_object, video_segment_duration = read_mpd.read_mpd(mpd_file, dp_object)
    config_dash.LOG.info("The DASH media has %d video representations" % len(dp_object.video))
    if LIST:
        # Print the representations and EXIT
        print_representations(dp_object)
        return None
    if "all" in PLAYBACK.lower():
        if mpd_file:
            config_dash.LOG.critical("Start ALL Parallel PLayback")
            start_playback_all(dp_object, domain)
    elif "basic" in PLAYBACK.lower():
        config_dash.LOG.critical("Start Basic-DASH Playback")
        start_playback_smart(dp_object, domain, "BASIC", DOWNLOAD, video_segment_duration)
    elif "smart" in PLAYBACK.lower():
        config_dash.LOG.critical("Start Smart-DASH Playback: Weighted Harmonic mean")
        start_playback_smart(dp_object, domain, "SMART", DOWNLOAD, video_segment_duration)
    else:
        config_dash.LOG.error("Unknown Playback parameter {}".format(PLAYBACK))
        return None

if __name__ == "__main__":
    sys.exit(main())