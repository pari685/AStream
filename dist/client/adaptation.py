"""
Adaptation algorithms
1. basic_dash
2. weighted_dash
"""
from __future__ import division
import config_dash


def calculate_rate_index(bitrates, curr_rate):
    """ Module that finds the bitrate closes to the curr_rate
    :param bitrates: LIst of available bitrates
    :param curr_rate: current_bitrate
    :return: curr_rate_index
    """
    if curr_rate < bitrates[0]:
        return bitrates[0]
    elif curr_rate > bitrates[-1]:
        return bitrates[-1]
    else:
        for bitrate, index in enumerate(bitrates[1:]):
            if bitrates[index-1] < curr_rate < bitrate:
                return curr_rate


def basic_dash(segment_number, bitrates, average_dwn_time,
               segment_download_time, curr_rate):
    """
    Module to predict the next_bitrate using the basic_dash algorithm
    :param segment_number: Current segment number
    :param bitrates: A tuple/list of available bitrates
    :param average_dwn_time: Average download time observed so far
    :param segment_download_time:  Time taken to download the current segment
    :param curr_rate: Current bitrate being used
    :return: next_rate : Bitrate for the next segment
    :return: updated_dwn_time: Updated average downlaod time
    """

    if average_dwn_time > 0 and segment_number > 0:
        updated_dwn_time = (average_dwn_time * (segment_number + 1) + segment_download_time) / (segment_number + 1)
    else:
        updated_dwn_time = segment_download_time
    config_dash.LOG.debug("The average download time upto segment {} is {}. Before it was {}".format(segment_number,
                                                                                                     updated_dwn_time,
                                                                                                     average_dwn_time))

    bitrates = [float(i) for i in bitrates]
    bitrates.sort()
    try:
        sigma_download = average_dwn_time / segment_download_time
        config_dash.LOG.debug("Sigma Download = {}/{} = {}".format(average_dwn_time, segment_download_time,
                                                                   sigma_download))
    except ZeroDivisionError:
        config_dash.LOG.error("Download time = 0. Unable to calculate the sigma_download")
        return curr_rate, updated_dwn_time
    try:
        curr = bitrates.index(curr_rate)
    except ValueError:
        config_dash.LOG.error("Current Bitrate not in the bitrate lsit. Setting to minimum")
        curr = calculate_rate_index(bitrates, curr_rate)
    next_rate = curr_rate
    if sigma_download < 1:
        if curr > 0:
            if sigma_download < bitrates[curr - 1]/bitrates[curr]:
                next_rate = bitrates[0]
            else:
                next_rate = bitrates[curr - 1]
    elif curr_rate < bitrates[-1]:
        if sigma_download >= bitrates[curr - 1]/bitrates[curr]:
            temp_index = curr
            while next_rate < bitrates[-1] or sigma_download < (bitrates[curr+1] / bitrates[curr]):
                temp_index += 1
                next_rate = bitrates[temp_index]
    return next_rate, updated_dwn_time


def basic_dash2(segment_number, bitrates, average_dwn_time,
                segment_download_time, total_downloaded):
    """
    Module to predict the next_bitrate using the basic_dash algorithm
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


class WeightedMean:
    """ Harmonic mean.
        The weights are the sizes of the segments
    """
    def __init__(self):
        # List of (size, download_rate)
        self.segment_info = list()
        self.weighted_mean_rate = 0

    def update_weighted_mean(self, segment_size, segment_download_time):
        """ Method to update the weighted harmonic mean for the segments.
            segment_size is in bytes
            segment_download_time is in seconds
            http://en.wikipedia.org/wiki/Harmonic_mean#Weighted_harmonic_mean
        """
        segment_download_rate = segment_size / segment_download_time
        self.segment_info.append((segment_size, segment_download_rate))
        self.weighted_mean_rate = sum([size for size, _ in self.segment_info]) / sum([s/r for s, r in self.segment_info])
        return self.weighted_mean_rate


def weighted_dash(bitrates, dash_player, weighted_dwn_rate, curr_bitrate, next_segment_sizes):
    """
    Module to predict the next_bitrate using the weighted_dash algorithm
    :param bitrates: List of bitrates
    :param weighted_dwn_rate:
    :param curr_bitrate:
    :param next_segment_sizes: A dict mapping bitrate: size of next segment
    :return: next_bitrate, delay
    """
    bitrates = [int(i) for i in bitrates]
    bitrates.sort()
    # Waiting time before downloading the next segment
    delay = 0
    next_bitrate = None
    available_video_segments = dash_player.buffer.qsize() - dash_player.initial_buffer
    # If the buffer is less that the Initial buffer, playback remains at th lowest bitrate
    # i.e dash_buffer.current_buffer < dash_buffer.initial_buffer
    available_video_duration = available_video_segments * dash_player.segment_duration
    config_dash.LOG.debug("Buffer_length = {} Initial Buffer = {} Available video = {} seconds, alpha = {}. "
                          "Beta = {} WDR = {}, curr Rate = {}".format(dash_player.buffer.qsize(),
                                                                      dash_player.initial_buffer,
                                                                      available_video_duration, dash_player.alpha,
                                                                      dash_player.beta, weighted_dwn_rate,
                                                                      curr_bitrate))

    if weighted_dwn_rate == 0 or available_video_segments == 0:
        next_bitrate = bitrates[0]
    # If time to download the next segment with current bitrate is longer than current - initial,
    # switch to a lower suitable bitrate

    elif float(next_segment_sizes[curr_bitrate])/weighted_dwn_rate > available_video_duration:
        config_dash.LOG.debug("next_segment_sizes[curr_bitrate]) weighted_dwn_rate > available_video")
        for bitrate in reversed(bitrates):
            if bitrate < curr_bitrate:
                if float(next_segment_sizes[bitrate])/weighted_dwn_rate < available_video_duration:
                    next_bitrate = bitrate
                    break
        if not next_bitrate:
            next_bitrate = bitrates[0]
    elif available_video_segments <= dash_player.alpha:
        config_dash.LOG.debug("available_video <= dash_player.alpha")
        if curr_bitrate >= max(bitrates):
            config_dash.LOG.info("Current bitrate is MAX = {}".format(curr_bitrate))
            next_bitrate = curr_bitrate
        else:
            higher_bitrate = bitrates[bitrates.index(curr_bitrate)+1]
            # Jump only one if suitable else stick to the current bitrate
            config_dash.LOG.info("next_segment_sizes[higher_bitrate] = {}, weighted_dwn_rate = {} , "
                                 "available_video={} seconds, ratio = {}".format(next_segment_sizes[higher_bitrate],
                                                                                 weighted_dwn_rate,
                                                                                 available_video_duration,
                                                                                 float(next_segment_sizes[
                                                                                     higher_bitrate])/weighted_dwn_rate))
            if float(next_segment_sizes[higher_bitrate])/weighted_dwn_rate < available_video_duration:
                next_bitrate = higher_bitrate
            else:
                next_bitrate = curr_bitrate
    elif available_video_segments <= dash_player.beta:
        config_dash.LOG.debug("available_video <= dash_player.beta")
        if curr_bitrate >= max(bitrates):
            next_bitrate = curr_bitrate
        else:
            for bitrate in reversed(bitrates):
                if bitrate >= curr_bitrate:
                    if float(next_segment_sizes[bitrate])/weighted_dwn_rate < available_video_duration:
                        next_bitrate = bitrate
                        break
            if not next_bitrate:
                next_bitrate = curr_bitrate

    elif available_video_segments > dash_player.beta:
        config_dash.LOG.debug("available_video > dash_player.beta")
        if curr_bitrate >= max(bitrates):
            next_bitrate = curr_bitrate
        else:
            for bitrate in reversed(bitrates):
                if bitrate >= curr_bitrate:
                    if float(next_segment_sizes[bitrate])/weighted_dwn_rate > available_video_duration:
                        next_bitrate = bitrate
                        break
        if not next_bitrate:
            next_bitrate = curr_bitrate
        delay = dash_player.buffer.qsize() - dash_player.beta
        config_dash.LOG.info("Delay:{}".format(delay))
    else:
        next_bitrate = curr_bitrate
    config_dash.LOG.debug("The next_bitrate is assigned as {}".format(next_bitrate))
    return next_bitrate, delay