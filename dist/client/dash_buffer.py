import Queue
import time
import dash_config

# Duartions in seconds
SEGMENT_DURATION = 4
INITIAL_BUFFERING_DURATION = 5

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
        #PRE_START, INITIAL_BUFFERING, PLAY, PAUSE, BUFFERING, STOP, END
        self.playback_start_time = None
        self.current_playback_time = 0
        self.playback_duration = video_length
        self.playback_state = "PRE-START"
        self.user_action = None
        self.max_buffer_size = video_length
        self.current_buffer = Queue.Queue()
        self.buffer = Queue.Queue()
        self.current_segment = None

    def player_time(self):
        """ function that update the current playback time"""
        start_player = time.time()
        while not self.current_buffer.empty():
            if self.playback_state == "STOP":
                # If video is stopped quit updating the playback time and exit player
                config_dash.LOG.INFO("Player Stopped")
                return 0
            elif self.playback_state == "PAUSE":
                # do not update the playkack time. Wait for the state to change
                continue
            if self.playback_state == "INITIAL_BUFFERING":
                if self.current_buffer.qsize() * SEGMENT_DURATION < INITIAL_BUFFERING_DURATION:
                    initial_wait = time.time() - start_player
                    continue
                else:
                    now = time.time()
                    future = now + self.current_buffer.get()
                    while time.time() < future:
                        if not self.playback_start_time:
                            self.playback_start_time = time.time()
                            print "Started Playback"
                        int_time = int(time.time() - self.playback_start_time)
                        if self.current_playback_time < int_time:
                            self.current_playback_time = int_time
                        if self.current_playback_time >= self.playback_duration:
                            print "Completed Video Playback"
                            return 1


    def player_manager(self):
        while True:
            if playback_state == "INITIAL_BUFFERING":
                if self.current_buffer >= INITIAL_BUFFER_DURATION:
                    playback_state = "PLAY"
            if playback_state in ["PLAY", "BUFFERING"]:
                if not self.buffer.empty():
                    self.current_segment = self.buffer.get()
                    self.current_buffer.put(current_segment['playback_length'])












    def write(self, segment):
        """ write segment to the buffer"""
        self.buffer.put(segment)
        self.current_buffer += int(segment[playback_length])

    def start(self):
        """ Start playback"""
        self.playback_state = "PLAY"









