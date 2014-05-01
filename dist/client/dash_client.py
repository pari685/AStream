#!/usr/local/bin/python
"""
Author:            Parikshit Juluri
Contact:           pjuluri@mail.umkc.edu

Testing:
    import dash_client
    mpd_file = <MPD_FILE>
    dash_client.playback_duration(mpd_file, 'http://198.248.242.16:8005/')
"""
import read_mpd
import urlparse
import urllib2
import random
import os
import sys
import errno
import timeit
import time
import httplib
from argparse import ArgumentParser
from multiprocessing import Process, Queue
from collections import defaultdict
# GLobals for arg parser with the default values
# Not sure if this is the correct way ....
MPD = 'http://198.248.242.16:8006/media/mpd/x4ukwHdACDw.mpd'
LIST = False
PLAYBACK = 'all'

ASCII_UPPERCASE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ASCII_DIGITS = '0123456789'

def get_mpd(url):
    """ Module to download the MPD from the URL and save it to file"""
    try:
        connection = urllib2.urlopen(url, timeout=10)
    except urllib2.HTTPError, error:
        print error.code
        print "Unable to download MPD file HTTP Error.",
        return None
    except urllib2.URLError:
        print '''URLError. Unable to reach Server.  
        Check if Server active '''
        return None
    except IOError, httplib.HTTPException:
        print "Unable to , file_identifierdownload MPD file HTTP Error."
        return None
    
    mpd_data = connection.read()
    connection.close()
    mpd_file = url.split('/')[-1]
    mpd_file_handle = open(mpd_file, 'w')
    mpd_file_handle.write(mpd_data)
    mpd_file_handle.close()
    return mpd_file

def get_bandwidth(data, duration):
    """ Module to determine the bandwidth for a segment
    download"""
    return (data*8/duration)

def get_domain_name(url):
    """ Module to obtain the domain name from the URL
        From : http://stackoverflow.com/questions/9626535/get-domain-name-from-url
    """
    parsed_uri = urlparse.urlparse(url)
    domain = '{uri.scheme}://{uri.netloc}/'.format(uri=parsed_uri)
    return domain

def id_generator(size=6):
    """ Module to create a random string with uppercase 
        and digits.
    """
    chars =  ASCII_UPPERCASE + ASCII_DIGITS
    return 'TEMP_' + ''.join(random.choice(chars) for _ in range(size))


def download_segment_single_folder(segment_url, dash_folder):
    """ Module to download the segement"""
    try:
        connection = urllib2.urlopen(segment_url)
    except urllib2.HTTPError, error:
        print error.code
        print "Unable to download DASH Segment file HTTP Error"
        return None
    parsed_uri = urlparse.urlparse(segment_url)
    segment_path = '{uri.path}'.format(uri=parsed_uri)
    while segment_path.startswith('/'):
        segment_path = segment_path[1:]
        print "SEGMENT PATH", segment_path
    segment_filename = os.path.join(dash_folder,
            os.path.basename(segment_path))
    make_sure_path_exists(os.path.dirname(segment_filename))
    segment_file_handle = open(segment_filename, 'wb')
    segment_file_handle.write(connection.read())
    connection.close()
    segment_file_handle.close()
    return segment_filename

def download_segment(segment_url, file_identifier):
    """ Module to download the segement"""
    try:
        connection = urllib2.urlopen(segment_url)
    except urllib2.HTTPError, error:
        print error.code
        print "Unable to download DASH Segment file HTTP Error"
        return None
    parsed_uri = urlparse.urlparse(segment_url)
    segment_path = '{uri.path}'.format(uri=parsed_uri)
    while segment_path.startswith('/'):
        segment_path = segment_path[1:]
        print "SEGMENT PATH", segment_path
    segment_filename = os.path.join(file_identifier,
            segment_path)
    make_sure_path_exists(os.path.dirname(segment_filename))
    segment_file_handle = open(segment_filename, 'wb')
    segment_file_handle.write(connection.read())
    connection.close()
    segment_file_handle.close()
    return segment_filename

def get_media_all(domain, media_info, file_identifier, done_queue):
    """ Download the media from the list of URL's in media
    http://toastdriven.com/blog/2008/nov/11/brief-introduction-multiprocessing/
    """
    bandwidth, media_dict = media_info
    media = media_dict[bandwidth]
    print "GET MEDIA", file_identifier
    media_start_time = timeit.default_timer()
    for segment in [media.initialization] + media.url_list:
        start_time = timeit.default_timer()
        segment_url = urlparse.urljoin(domain, segment)
        segment_file = download_segment(segment_url,
                                        file_identifier)
        elapsed = timeit.default_timer() - start_time
        if segment_file:
            done_queue.put((bandwidth, segment_url, elapsed))
        print "Downloaded Segment bandwidth %d URL %s" % (
                bandwidth, segment)
    media_download_time = timeit.default_timer() - media_start_time
    done_queue.put((bandwidth, 'STOP', media_download_time))
    return None

def make_sure_path_exists(path):
    """ Module to make sure the path exists if not create it
    """
    print "Trying to create the path", path
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise

def print_representations(dp_object):
    """ Module to print the representations"""
    print "The DASH media has the following audio representations"
    for bandwidth in dp_object.audio:
        print bandwidth
    print "The DASH media has the following video representations"
    for bandwidth in dp_object.video:
        print bandwidth

class DASHPlayback(object):
    """ Function to simulate the playback
        segment_info = {'url' = None,
        'bandwidth' = None
    """
    _playback_segments = Queue()
    _completed_segments = list()
    _playback_process = None

    def __init__(self, segment_duration):
        self.playback_cursor = 0.0
        self.start_time = 0.0
        self.interruptions = 0.0
        self.total_inter_duration = 0.0
        self.current_segment = None
        try:
            self.segment_duration = int(segment_duration)
        except ValueError:
            print "handle_segment_duration"

    def pb_add_to_queue(self, segment_info):
        """ Module to add segemnt url to queue"""
        self._playback_segments.put(segment_info)

    def playback(self):
        """ Module that does playback"""
        for segment in iter(self._playback_segments.get, None):
            self.current_segment = segment
            time.sleep(self.segment_duration)
            self._completed_segments.append(segment)
            print "Completed playback of :", segment

    def start_playback(self):
        """ Module to start playback"""
        self.start_time = timeit.default_timer()
        self._playback_process = Process(
                                target=self.playback)
    def stop_playback(self):
        """ Module to stop the playback"""
        # TODO

def start_playback_smart(dp_object, domain):
    """ Module that downloads the MPD-FIle and download
        all the representations of the Module to download
        the MPEG-DASH media.
    """
    audio_done_queue = Queue()
    video_done_queue = Queue()
    processes = []
    file_identifier = id_generator(6)
    print "FILE IDENT", file_identifier

    #for bandwidth in dp_object.audio:
    #    dp_object.audio[bandwidth] = read_mpd.get_url_list(
    #            bandwidth, dp_object.audio[bandwidth],
    #            dp_object.playback_duration)
    #    process = Process(target=get_media, args=(domain,
    #            (bandwidth, dp_object.audio), 
    #            file_identifier,
    #            audio_done_queue))
    #    process.start()
    #    processes.append(process)
    dp_list = defaultdict(defaultdict)
    for bandwidth in dp_object.video:
        dp_object.video[bandwidth] = read_mpd.get_url_list(
                bandwidth, dp_object.video[bandwidth],
                dp_object.playback_duration)
        media_url = [dp_object.video[bandwidth].initialization] + dp_object.video[bandwidth].url_list
        for segment_count, segment_url in enumerate(media_url):
            print segment_url, segment_count
            dp_list[segment_count][bandwidth] = segment_url
    bandwidths = dp_object.video.keys()
    #print dp_list
    for segment in dp_list:
        segment_path =  dp_list[segment][bandwidths[0]]
        segment_url = urlparse.urljoin(domain, 
                segment_path)
        start_time = timeit.default_timer()
        try:
            download_segment_single_folder(segment_url, file_identifier)
        except IOError, e:
            print "Unable to save segement"
            return None
        duration = timeit.default_timer() - start_time 
        print ">>>>>>", dp_list[segment][349683], duration

def start_playback_all(dp_object, domain):
    """ Module that downloads the MPD-FIle and download
        all the representations of the Module to download
        the MPEG-DASH media.
    """
    audio_done_queue = Queue()
    video_done_queue = Queue()
    processes = []
    
    file_identifier = id_generator(6)
    print "FILE IDENT", file_identifier

    for bandwidth in dp_object.audio:
        # Get the list of URL's (relative location) for the audio 
        dp_object.audio[bandwidth] = read_mpd.get_url_list(
                bandwidth, dp_object.audio[bandwidth],
                dp_object.playback_duration)
        # Create a new process to download the audio stream.
        # The domain + URL from the above list gives the 
        # complete path
        # The fil-identifier is a random string used to 
        # create  a temporary folder for current session
        # Audio-done queue is used to excahnge information
        # between the process and the calling function.
        # 'STOP' is added to the queue to indicate the end 
        # of the download of the sesson
        process = Process(target=get_media_all, args=(domain,
                (bandwidth, dp_object.audio), 
                file_identifier,
                audio_done_queue))
        process.start()
        processes.append(process)

    for bandwidth in dp_object.video:
        dp_object.video[bandwidth] = read_mpd.get_url_list(
                bandwidth, dp_object.video[bandwidth],
                dp_object.playback_duration)
        #Create a new process to download the audio 
        #stream.
        #The domain + URL from the above list gives the
        #complete path
        #The file-identifier is a random string used to
        #create a temporary folder for current session
        #Video-done queue is used to excahnge information
        #between the process and the calling function.
        #'STOP' is added to the queue to indicate the 
        #end of the download of the sesson

        process = Process(target=get_media_all, args=(domain,
                (bandwidth, dp_object.video),
                file_identifier, video_done_queue))
        process.start()
        processes.append(process)

    for process in processes:
        process.join()

    count = 0
    for queue_values in iter(video_done_queue.get, None):
        bandwidth, status, elapsed = queue_values
        if status == 'STOP':
            print "Completed download of %s in %f " % (
                    bandwidth, elapsed)
            count += 1
            if count == len(dp_object.video):
                # If the download of all the videos is done
                # the stop the
                print "Finished d/w of  all video segments"
                break

def create_arguments(parser):
    """ Adding arguments to the parser"""
    parser.add_argument('-m', '--MPD',
            help=("Url to the MPD File"))
    parser.add_argument('-l', '--LIST', action='store_true',
            help=("List all the representations"))
    parser.add_argument('-p', '--PLAYBACK',
            help="Playback type (all, or smart)")

def update_config(args):
    """ Module to update the config values with the arguments""" 
    globals().update(vars(args))
    return 

def main():
    """ Main Program wrapper"""
    parser = ArgumentParser(description='Process Client paameters')
    create_arguments(parser)
    args = parser.parse_args()
    update_config(args)
    if not MPD:
        print "ERROR: Please provide the URL to the MPD file. Try Again.."
        return
    print 'Downloading MPD file from %s %s' % (MPD, LIST)
    mpd_file = get_mpd(MPD)
    domain = get_domain_name(MPD)
    print 'Starting the streaming of the mpd_file'
    dp_object = read_mpd.DashPlayback()
    dp_object = read_mpd.read_mpd(mpd_file, dp_object)
    #playback_duration = dp_object.playback_duration
    #dash_audio = dp_object.audio
    print "The DASH media has %d audio representations" % (
            len(dp_object.audio))
    #dash_video = dash_playback_object.video
    print "The DASH media has %d video representations" % (
                                len(dp_object.video))
    if LIST:
        print_representations(dp_object)
        return None

    if "all" in PLAYBACK.lower():
        if mpd_file:
            start_playback_all(dp_object, domain)
    elif "smart" in PLAYBACK.lower():
        print "Start SMART PLayback"
        start_playback_smart(dp_object, domain)


if __name__ == "__main__":
    sys.exit(main())
