import Queue
import threading
import time
import config_dash
import logging

from configure_log_file import configure_log_file

if not config_dash.LOG:
    configure_log_file()
# Durations in seconds
SEGMENT_DURATION = 20
INITIAL_BUFFERING_DURATION = 5
PLAYER_STATES = ['INITIALIZED', 'INITIAL_BUFFERING', 'PLAY',
                 'PAUSE', 'BUFFERING', 'STOP', 'END']
# segment = {'playback_length' : 4,
#            'size' : 1024,
#            'bitrate': 120
#            'data': "<byte data>",
#            'URI': "URL of the segment"}


class DashBuffer:
    """ DASH buffer class
        TODO: Progressbar https://github.com/slok/pygressbar
    """
    def __init__(self, video_length):
        self.player_thread = None
        self.playback_start_time = None
        self.current_playback_time = 0
        self.playback_duration = video_length
        self.playback_state = "INITIALIZED"
        # Unlimited Buffer size
        self.max_buffer_size = video_length
        # Duration of the current buffer
        self.buffer_length = 0
        self.buffer_length_lock = threading.RLock()
        # Current buffer that holds the segment data
        self.buffer = Queue.Queue()
        self.buffer_lock = threading.RLock()
        self.current_segment = None

    def get_state(self):
        """ Function that returns the current state of the player"""
        return self.playback_state

    def set_state(self, state):
        """ Function to set the state of the player"""
        state = state.upper()
        if state in PLAYER_STATES:
            self.playback_state = state
            config_dash.LOG.info("State set to {}".format(state))
        else:
            config_dash.LOG.error("Unidentified state: {}".format(state))

    def initialize_player(self):
        """ function that update the current playback time"""
        start_time = time.time()
        initial_wait = 0
        #while not self.current_buffer.empty():
        while True:
            if self.playback_state == "STOP":
                # If video is stopped quit updating the playback time and exit player
                config_dash.LOG.info("Player Stopped at time {}".format(
                    time.time() - start_time))
                return 0
            elif self.playback_state == "PAUSE":
                # do not update the playback time. Wait for the state to change
                config_dash.LOG.info("Player Paused after {} seconds of playback".format(self.current_playback_time))
                continue
            if self.playback_state == "INITIAL_BUFFERING":
                if self.buffer.qsize() * SEGMENT_DURATION < INITIAL_BUFFERING_DURATION:
                    initial_wait = time.time() - start_time
                    continue
                else:
                    config_dash.LOG.info("Initial Waiting Time = {}".format(initial_wait))
                    self.playback_state = "PLAY"
            if self.playback_state == "PLAY":
                    now = time.time()
                    # Acquire Lock on the buffer and read a segment for it
                    self.buffer_lock.acquire()
                    play_segment = self.current_buffer.get()
                    print play_segment
                    self.buffer_lock.release()
                    # time when the segment finishes playback
                    future = now + play_segment['playback_length']
                    while time.time() < future:
                        # If playback hasn't started yet, set the playback_start_time
                        if not self.playback_start_time:
                            self.playback_start_time = time.time()
                            config_dash.LOG.info("Started playing with representation {}".format(
                                segment['bitrate']))
                        # Duration for which the video was played in seconds (integer)
                        int_time = int(time.time() - self.playback_start_time)
                        if self.current_playback_time < int_time:
                            self.current_playback_time = int_time
                        else:
                            config_dash.LOG.error("Playback time ahead")
                        if self.current_playback_time >= self.playback_duration:
                            config_dash.LOG.info("Completed the video playback: {} seconds".format(self.current))
                            return 1
                    else:
                        self.buffer_length_lock.release()
                        self.buffer_length -= play_segment['playback_length']
                        self.buffer_length_lock.release()

    def write(self, segment):
        """ write segment to the buffer.
            Segment is dict with keys ['data', 'bitrate', 'playback_length', 'URI', 'size']
        """
        # Acquire Lock on the buffer and add a segment to it
        self.buffer_lock.acquire()
        self.buffer.put(segment)
        self.buffer_lock.release()
        self.buffer_length_lock.acquire()
        self.buffer_length += int(segment['playback_length'])
        self.buffer_length_lock.release()

    def start(self):
        """ Start playback"""
        self.playback_state = "INITIAL_BUFFERING"
        config_dash.LOG.info("Starting the playback")
        self.player_thread = threading.Thread(target=self.initialize_player)
        self.player_thread.start()











