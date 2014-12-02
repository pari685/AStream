""" Module for an MOD client
    Author: Parikshit Juluri
    Contact : pjuluri@umkc.edu

"""
import re
import config_dash

# Try to import the C implementation of ElementTree which is faster
# In case of ImportError import the pure Python implementation
try:
    import xml.etree.cElementTree as ET
except ImportError:
    import xml.etree.ElementTree as ET

MEDIA_PRESENTATION_DURATION = 'mediaPresentationDuration'
MIN_BUFFER_TIME = 'minBufferTime'


def get_tag_name(xml_element):
    """ Module to remove the xmlns tag from the name"""
    return xml_element[xml_element.find('}')+1:]


def get_playback_time(playback_duration):
    """ Get the playback time(in seconds) from the string:
        Eg: PT0H1M59.89S
    """
    # Get all the numbers in the string
    numbers = re.split('[PTHMS]', playback_duration)
    #remove all the empty strings
    numbers = [value for value  in numbers if value != '']
    numbers.reverse()
    total_duration = 0
    for count, val in enumerate(numbers):
        if count == 0:
            total_duration += float(val)
        elif count == 1:
            total_duration += float(val) * 60
        elif count == 2:
            total_duration += float(val) * 60 * 60
    return total_duration


class MediaObject(object):
    """Object to handel audio and video stream """
    def __init__(self):
        self.min_buffer_time = None
        self.start = None
        self.timescale = None
        self.segment_duration = None
        self.initialization = None
        self.base_url = None
        self.url_list = list()


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


def get_url_list(bandwidth, media, playback_duration):
    segment_playback = media.segment_duration / media.timescale
    total_playback = 0
    segment_count = media.start
    # Get the Base URL string
    base_url = media.base_url
    base_url = base_url.split('$')
    base_url[1] = base_url[1].replace('$', '')
    base_url[1] = base_url[1].replace('Number', '')
    base_url = ''.join(base_url)
    while total_playback < playback_duration:
        media.url_list.append(base_url % (segment_count))
        segment_count += 1
        total_playback += segment_playback
    return media

def read_mpd(mpd_file, dashplayback):
    """ Module to read the MPD file"""
    config_dash.LOG.info("Reading the MPD file")
    try:
        tree = ET.parse(mpd_file)
    except IOError:
        config_dash.LOG.error("MPD file not found. Exiting")
        return None
    root = tree.getroot()
    if 'MPD' in get_tag_name(root.tag).upper():
        if MEDIA_PRESENTATION_DURATION in root.attrib:
            dashplayback.playback_duration = get_playback_time(
                    root.attrib[MEDIA_PRESENTATION_DURATION])
        if MIN_BUFFER_TIME in root.attrib:
            dashplayback.min_buffer_time = get_playback_time(root.attrib[MIN_BUFFER_TIME])
    child_period = root[0]

    for adaptation_set in child_period:
        if 'mimeType' in adaptation_set.attrib:
            media_type = None
            media_found = False
            if 'audio' in adaptation_set.attrib['mimeType']:
                media_type = dashplayback.audio
                media_found = True
                config_dash.LOG.info("Found Audio")
            elif 'video' in adaptation_set.attrib['mimeType']:
                media_type = dashplayback.video
                media_found = True
                config_dash.LOG.info("Found Video")
            if media_found:
                config_dash.LOG.info("Retrieving Media")
                for representation in adaptation_set:
                    bandwidth = int(representation.attrib['bandwidth'])
                    media_type[bandwidth] = MediaObject()
                    for segment_template in representation:
                        if 'duration' in segment_template.attrib:
                            media_type[bandwidth].segment_duration = int(segment_template.attrib['duration'])
                            media_type[bandwidth].base_url = segment_template.attrib['media']
                            media_type[bandwidth].start = int(segment_template.attrib['startNumber'])
                            media_type[bandwidth].timescale = int(segment_template.attrib['timescale'])
                            media_type[bandwidth].initialization = segment_template.attrib['initialization']
    return dashplayback
