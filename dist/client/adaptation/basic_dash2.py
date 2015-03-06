__author__ = 'pjuluri'

import config_dash


def basic_dash2(segment_number, bitrates, average_dwn_time,
                segment_download_time, total_downloaded):
    """
    Module to predict the next_bitrate using the basic_dash algorithm. Selects the bitrate that is one lower than the
    current network capacity.
    :param segment_number: Current segment number
    :param bitrates: A tuple/list of available bitrates
    :param average_dwn_time: Average download time observed so far
    :param segment_download_time:  Time taken to download the current segment
    :return: next_rate : Bitrate for the next segment
    :return: updated_dwn_time: Updated average downlaod time
    """
    if average_dwn_time > 0 and segment_number > 0:
        updated_dwn_time = (average_dwn_time * segment_number + segment_download_time) / (segment_number + 1)
    else:
        updated_dwn_time = segment_download_time
    config_dash.LOG.debug("The average download time upto segment {} is {}. Before it was {}".format(segment_number,
                                                                                                     updated_dwn_time,
                                                                                                     average_dwn_time))
    # Calculate the download_rate in Kbps
    download_rate = total_downloaded * 8 / (updated_dwn_time * (segment_number + 1))
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

