import Queue
import threading
import time
import config_dash
from stop_watch import StopWatch
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
        self.playback_duration = video_length
        # Timers to keep trcak of playback time and the actual time
        self.playback_timer = StopWatch()
        self.actual_start_time = None
        # Playback State
        self.playback_state = "INITIALIZED"
        self.playback_state_lock = threading.Lock()
        # Buffer size
        self.max_buffer_size = video_length
        # Duration of the current buffer
        self.buffer_length = 0
        self.buffer_length_lock = threading.Lock()
        # Current video buffer that holds the segment data
        self.buffer = Queue.Queue()
        self.buffer_lock = threading.Lock()
        self.current_segment = None

    def get_state(self):
        """ Function that returns the current state of the player"""
        return self.playback_state

    def set_state(self, state):
        """ Function to set the state of the player"""
        state = state.upper()
        if state in PLAYER_STATES:
            self.playback_state_lock.acquire()
            config_dash.LOG.info("Changing state from {} to {}".format(self.playback_state, state))
            self.playback_state = state
            self.playback_state_lock.release()
        else:
            config_dash.LOG.error("Unidentified state: {}".format(state))

    def initialize_player(self):
        """Method that update the current playback time"""
        start_time = time.time()
        initial_wait = 0
        paused = False
        #while not self.current_buffer.empty():
        while True:
            if self.playback_state == "STOP":
                # If video is stopped quit updating the playback time and exit player
                config_dash.LOG.info("Player Stopped at time {}".format(
                    time.time() - start_time))
                self.playback_timer.pause()
                return "STOPPED"

            if self.playback_state == "PAUSE":
                if not paused:
                    # do not update the playback time. Wait for the state to change
                    config_dash.LOG.info("Player Paused after {} seconds of playback".format(self.playback_timer.time()))
                    self.playback_timer.pause()
                    paused = True
                continue

            if self.playback_state == "INITIAL_BUFFERING":
                if self.buffer.qsize() * SEGMENT_DURATION < INITIAL_BUFFERING_DURATION:
                    initial_wait = time.time() - start_time
                    continue
                else:
                    config_dash.LOG.info("Initial Waiting Time = {}".format(initial_wait))
                    self.set_state("PLAY")
                    config_dash.LOG.info("Changed state from INITIAL_BUFFERING to PLAY")

            if self.playback_state == "PLAY":
                    # Acquire Lock on the buffer and read a segment for it
                    self.buffer_lock.acquire()
                    play_segment = self.buffer.get()
                    print play_segment
                    self.buffer_lock.release()
                    # time when the segment finishes playback
                    future = self.playback_timer.time() + play_segment['playback_length']
                    while self.playback_timer.time() < future:
                        # If playback hasn't started yet, set the playback_start_time
                        if not self.playback_start_time:
                            self.playback_start_time = time.time()
                            self.playback_timer.start()
                            config_dash.LOG.info("Started playing with representation {}".format(
                                play_segment['bitrate']))
                        # Duration for which the video was played in seconds (integer)
                        if self.playback_timer.time() >= self.playback_duration:
                            config_dash.LOG.info("Completed the video playback: {} seconds".format(
                                self.playback_duration))
                            return 1
                    else:
                        self.buffer_length_lock.acquire()
                        self.buffer_length -= int(play_segment['playback_length'])
                        self.buffer_length_lock.release()

    def write(self, segment):
        """ write segment to the buffer.
            Segment is dict with keys ['data', 'bitrate', 'playback_length', 'URI', 'size']
        """
        # Acquire Lock on the buffer and add a segment to it
        config_dash.LOG.info("Writing segment at time {}".format(time))
        self.buffer_lock.acquire()
        self.buffer.put(segment)
        self.buffer_lock.release()
        self.buffer_length_lock.acquire()
        self.buffer_length += int(segment['playback_length'])
        self.buffer_length_lock.release()

    def start(self):
        """ Start playback"""
        self.set_state("INITIAL_BUFFERING")
        config_dash.LOG.info("Starting the playback")
        self.player_thread = threading.Thread(target=self.initialize_player)
        self.player_thread.start()

    def stop(self):
        """Method to stop the playback"""
        self.set_state("STOP")
        config_dash.LOG.info("Stopped the playback")