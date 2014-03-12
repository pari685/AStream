"""
Author:            Parikshit Juluri
Contact:           pjuluri@mail.umkc.edu 


"""

import read_mpd
import urllib2
import thread
from collections import defaultdict

from multiprocessing import Lock, Process, Queue, current_process

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




def get_media(media, done_queue):
    """ Download the media from the list of URL's in media
    http://toastdriven.com/blog/2008/nov/11/brief-introduction-multiprocessing/
    """
    print "media.url_list"
    print media.url_list
    for segment in [media.initialization] + media.url_list:
        print "Downloading segment %s" % (segment)
    return None


def start_playback(mpd_file):
    """ Module to download the MPEG-DASH media"""
    dash_playback_object = read_mpd.DashPlayback()
    dash_playback_object = read_mpd.read_mpd(mpd_file, dash_playback_object)
    playback_duration = dash_playback_object.playback_duration
    dash_audio = dash_playback_object.audio
    print "The DASH media has %d audio representations" % (len(dash_audio))
    dash_video = dash_playback_object.video
    print "The DASH media has %d video representations" % (len(dash_video))
    # Download audio and video (All representations)
    audio_done_queues = defaultdict(Queue)
    video_done_queues = defaultdict(Queue)
    
    processes = []

    for bandwidth in dash_audio:
        dash_audio[bandwidth]= read_mpd.get_url_list(bandwidth, dash_audio[bandwidth], playback_duration)
        p = Process(target=get_media, args=(dash_audio[bandwidth], audio_done_queues[bandwidth]))
        p.start()
        processes.append(p)

    for bandwidth in dash_video:
        dash_video[bandwidth]= read_mpd.get_url_list(bandwidth, dash_video[bandwidth], playback_duration)
        p = Process(target=get_media, args=(dash_video[bandwidth], video_done_queues[bandwidth]))
        p.start()
        processes.append(p)

    for p in processes:
        p.join()

    




    
    #for media in dash_playback_object.audio + dash_playback_object.video:

    #for bandwidth in dash_audio:
    #    read_mpd.get_url_list(bandwidth, dash_audio[bandwidth], dash_playback_object.playback_duration)
    #    thread.start_new_thread(get_media, (dash_audio[bandwidth],))
    #for bandwidth in dash_video:
    #    read_mpd.get_url_list(bandwidth, dash_video[bandwidth], dash_playback_object.playback_duration)
    #    thread.start_new_thread(get_media, (dash_video[bandwidth],))




#def client(threadName, delay):
#    """ Client """
#    count = 0
#    while count < 3:
#        time.sleep(delay)
#        count += 1
#        print "%s %s" % (threadName, delay)
#
#
#def start_thread():
#    """ Sample"""
#    try:
#        thread.start_new_thread (client, ("Thread-1", 2,) )
#        thread.start_new_thread (client, ("Thread-2", 4,) )
#    except:
#        print "Error: Unable to start thread"
#
#    while True:
#        pass
#
#
