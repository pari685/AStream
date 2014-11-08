import logging
import config_dash
import sys

def configure_log_file():
    """ Module to configure the log parameters
    and the log file.
    CRITICAL 50
    ERROR	40
    WARNING	30
    INFO	20
    DEBUG	10
    NOTSET	0
    """
    print "Configuring log file"
    config_dash.LOG = logging.getLogger(config_dash.LOG_NAME)
    config_dash.LOG_LEVEL = logging.DEBUG
    if config_dash.LOG_FILENAME == '-':
        handler = logging.StreamHandler(sys.stdout)
    else:
        handler = logging.FileHandler(filename=config_dash.LOG_FILENAME)
    config_dash.LOG.setLevel(config_dash.LOG_LEVEL)
    log_formatter = logging.Formatter('%(asctime)s - %(filename)s:%(lineno)d - %(levelname)s - %(message)s')
    handler.setFormatter(log_formatter)
    config_dash.LOG.addHandler(handler)