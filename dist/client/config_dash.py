from time import strftime
import os
# The configuration file for the AStream module
# create logger
LOG_NAME = 'astream_log'
LOG_LEVEL = None

# Set '-' to print to screen
LOG_FOLDER = "LOGS/"
if not os.path.exists(LOG_FOLDER):
    os.makedirs(LOG_FOLDER)

LOG_FILENAME = os.path.join(LOG_FOLDER, 'DASH_RUNTIME_LOG')
# Logs related to the statistics for the video
# PLAYBACK_LOG_FILENAME = os.path.join(LOG_FOLDER, strftime('DASH_PLAYBACK_LOG_%Y-%m-%d.%H_%M_%S.csv'))
# Buffer logs created by dash_buffer.py
BUFFER_LOG_FILENAME = os.path.join(LOG_FOLDER, strftime('DASH_BUFFER_LOG_%Y-%m-%d.%H_%M_%S.csv'))
LOG_FILE_HANDLE = None
# Set
LOG = None

# Constants for the BASIC-2 adaptation scheme
BASIC_THRESHOLD = 10
BASIC_UPPER_THRESHOLD = 1.2

# Constants for the Buffer in the Weighted adaptation scheme (in segments)
INITIAL_BUFFERING_COUNT = 1
RE_BUFFERING_COUNT = 1
ALPHA_BUFFER_COUNT = 5
BETA_BUFFER_COUNT = 10
# Set the size of the buffer in terms of segments. Set to unlimited if 0 or None
MAX_BUFFER_SIZE = None

# ---------------------------------------------------
# NETFLIX ADAPTATION
# ---------------------------------------------------
# Constants for the Netflix Buffering scheme adaptation/netflix_buffer.py
# Constants is terms of buffer occupancy percentage
NETFLIX_RESERVOIR = 0.1
NETFLIX_CUSHION = 0.9
# Buffer Size in Number of segments 240 seconds/4
NETFLIX_BUFFER_SIZE = 60
NETFLIX_INITIAL_BUFFER = 2
NETFLIX_INITIAL_FACTOR = 0.875

# For ping.py
PING_PACKETS = 10
ping_option_nb_pkts = PING_PACKETS
rtt_match = None
rtt_pattern = None
index_rtt_min = None
index_rtt_avg = None
index_rtt_max = None
RTT = False