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
import string
import random
import os
import errno
from multiprocessing import Process, Queue

def get_mpd(url):
    """ Module to download the MPD from the URL and save it to file"""
    try:
        mpd_data = urllib2.urlopen(url).read()
    except urllib2.HTTPError, e:
        print "Unable to download MPD file HTTP Error. Code", e.code
    mpd_file = url.split('/')[-1]
    mpd_file_handle = open(mpd_file, 'w')
    mpd_file_handle.write(mpd_data)
    mpd_file_handle.close()
    return mpd_file

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
    chars = string.ascii_uppercase + string.digits
    return ''.join(random.choice(chars) for _ in range(size))

def get_media(domain, media, file_identifier, done_queue):
    """ Download the media from the list of URL's in media
    http://toastdriven.com/blog/2008/nov/11/brief-introduction-multiprocessing/
    """
    print "GET MEDIA", file_identifier
    for segment in [media.initialization] + media.url_list:
        segment_url = urlparse.urljoin(domain, segment)
        segment_file = download_segment(segment_url,
                                        file_identifier)
        if segment_file:
            done_queue.put(segment_url)
        break
    done_queue.put('STOP')
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

def download_segment(segment_url, file_identifier):
    """ Module to download the segement"""
    try:
        segment_data = urllib2.urlopen(segment_url).read()
    except urllib2.HTTPError, e:
        print "Unable to download DASH Segment file HTTP Error. Code", e.code
        return None
    parsed_uri = urlparse.urlparse(segment_url)
    segment_path = '{uri.path}'.format(uri=parsed_uri)
    while segment_path.startswith('/'):
        segment_path = segment_path[1:]
        print "SEGMENT PATH", segment_path
    segment_filename = os.path.join(file_identifier,
            segment_path)
    make_sure_path_exists(os.path.dirname(segment_filename))
    print "Downloading segment %s to %s " % (segment_url,
                                            segment_filename)
    segment_file_handle = open(segment_filename, 'wb')
    segment_file_handle.write(segment_data)
    segment_file_handle.close()
    return segment_filename

def start_playback(mpd_file, domain):
    """ Module to download the MPEG-DASH media"""
    dash_playback_object = read_mpd.DashPlayback()
    dash_playback_object = read_mpd.read_mpd(mpd_file, dash_playback_object)
    playback_duration = dash_playback_object.playback_duration
    dash_audio = dash_playback_object.audio
    print "The DASH media has %d audio representations" % (len(dash_audio))
    dash_video = dash_playback_object.video
    print "The DASH media has %d video representations" % (len(dash_video))
    # Download audio and video (All representations)
    #audio_done_queues = defaultdict(Queue)
    #video_done_queues = defaultdict(Queue)
    audio_done_queue = Queue()
    video_done_queue = Queue()

    processes = []
    file_identifier = id_generator(6)
    print "FILE IDENT", file_identifier

    for bandwidth in dash_audio:
        dash_audio[bandwidth] = read_mpd.get_url_list(
                bandwidth, dash_audio[bandwidth],
                playback_duration)
        process = Process(target=get_media, args=(domain,
                dash_audio[bandwidth], file_identifier,
                audio_done_queue))
        process.start()
        processes.append(process)

    for bandwidth in dash_video:
        dash_video[bandwidth] = read_mpd.get_url_list(
                bandwidth, dash_video[bandwidth],
                playback_duration)
        process = Process(target=get_media, args=(domain,
                dash_video[bandwidth],file_identifier,
                video_done_queue))
        process.start()
        processes.append(process)

    for process in processes:
        process.join()

    for status in iter(video_done_queue.get, 'STOP'):
        print "Completed download of %s" %(status)

def main():
    """ Main Program wrapper"""
    url = 'http://198.248.242.16:8006/mpd/x4ukwHdACDw.mpd'
    print 'Downloading MPD file from %s' % (url)
    mpd_file = get_mpd(url)
    domian = get_domain_name(url)
    print 'Starting the streaming of the mpd_file'
    start_playback(mpd_file, domian)


