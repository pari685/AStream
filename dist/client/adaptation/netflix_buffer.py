from __future__ import division
__author__ = 'pjuluri'

"""
 The current module is the buffer based adaptaion scheme used by Netflix. Current design is based
 on the design from the paper:

    [1] Huang, Te-Yuan, et al. "A buffer-based approach to rate adaptation: Evidence from a large video streaming service."
    Proceedings of the 2014 ACM conference on SIGCOMM. ACM, 2014.
"""
try:
    from .. import config_dash
except ValueError:
    import config_dash
from collections import OrderedDict


def get_rate_netflix(current_buffer_occupancy, buffer_size=config_dash.NETFLIX_BUFFER_SIZE, bitrates):
    """
    Module that estimates the next bitrate basedon the rate map.
    Rate Map: Buffer Occupancy vs. Bitrates:
        If Buffer Occupancy < RESERVOIR (10%) :
            select the minimum bitrate
        if RESERVOIR < Buffer Occupancy < Cushion(90%) :
            Linear function
        if Buffer Occupancy > Cushion :
            Maximum Bitrate
    Ref. Fig. 6 from [1]

    :param current_buffer_occupancy: Current buffer occupancy in number of segments
    :param bitrates: List of available bitrates [r_min, .... r_max]
    :return:the bitrate for the next segment
    """
    next_bitrate = None
    try:
        bitrates = [int(i) for i in bitrates]
        bitrates.sort()
    except ValueError:
        config_dash.LOG.error("Unable to sort the bitrates. Assuming they are sorted")
    try:
        buffer_percentage = current_buffer_occupancy/buffer_size
        print buffer_percentage
    except ZeroDivisionError:
        config_dash.LOG.error("Buffer Size was found to be Zero")
        return None
    rate_map = OrderedDict()
    rate_map[config_dash.NETFLIX_RESERVOIR] = bitrates[0]
    intermediate_levels = bitrates[1:-1]
    marker_length = (config_dash.NETFLIX_CUSHION - config_dash.NETFLIX_RESERVOIR)/(len(intermediate_levels)+1)
    current_marker = config_dash.NETFLIX_RESERVOIR + marker_length
    for bitrate in intermediate_levels:
        rate_map[current_marker] = bitrate
        current_marker += marker_length
    rate_map[config_dash.NETFLIX_CUSHION] = bitrates[-1]
    if buffer_percentage <= config_dash.NETFLIX_RESERVOIR:
        next_bitrate = bitrates[0]
    elif buffer_percentage >= config_dash.NETFLIX_CUSHION:
        next_bitrate = bitrates[-1]
    else:
        config_dash.LOG.info("Rate Map: {}".format(rate_map))
        for marker in reversed(rate_map.keys()):
            if marker < buffer_percentage:
                break
            next_bitrate = rate_map[marker]
    return next_bitrate


def netflix_buffer(segment_number, bitrates, average_dwn_time,
                segment_download_time, total_downloaded):
    """
    Netflix rate adaptation module
    :param segment_number:
    :param bitrates:
    :param average_dwn_time:
    :param segment_download_time:
    :param total_downloaded:
    :return:
    """

