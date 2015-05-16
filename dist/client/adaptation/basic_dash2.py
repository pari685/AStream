__author__ = 'pjuluri'

import config_dash


def basic_dash2(segment_number, bitrates, average_dwn_time,
                recent_download_sizes, previous_segment_times):
    """
    Module to predict the next_bitrate using the basic_dash algorithm. Selects the bitrate that is one lower than the
    current network capacity.
    :param segment_number: Current segment number
    :param bitrates: A tuple/list of available bitrates
    :param average_dwn_time: Average download time observed so far
    :param segment_download_time:  Time taken to download the current segment
    :return: next_rate : Bitrate for the next segment
    :return: updated_dwn_time: Updated average download time
    """
    # Truncating the list of download times and segment sizes
    while len(previous_segment_times) > config_dash.BASIC_DELTA_COUNT:
        previous_segment_times.pop(0)
    while len(recent_download_sizes) > config_dash.BASIC_DELTA_COUNT:
        recent_download_sizes.pop(0)
    if len(previous_segment_times) == 0 or len(recent_download_sizes) == 0:
        return bitrates[0], None

    updated_dwn_time = sum(previous_segment_times) / len(previous_segment_times)

    config_dash.LOG.debug("The average download time upto segment {} is {}. Before it was {}".format(segment_number,
                                                                                                     updated_dwn_time,
                                                                                                     average_dwn_time))
    # Calculate the running download_rate in Kbps for the most recent segments
    download_rate = sum(recent_download_sizes) * 8 / (updated_dwn_time * len(previous_segment_times))
    bitrates = [float(i) for i in bitrates]
    bitrates.sort()
    next_rate = bitrates[0]
    for index, bitrate in enumerate(bitrates[1:], 1):
        if download_rate > bitrate:
            if download_rate > bitrate * config_dash.BASIC_UPPER_THRESHOLD:
                next_rate = bitrates[index]
            else:
                next_rate = bitrates[index - 1]
    config_dash.LOG.info("Download Rate = {}, next_bitrate = {}".format(download_rate, next_rate))
    return next_rate, updated_dwn_time

