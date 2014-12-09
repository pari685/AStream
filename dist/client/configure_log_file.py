import logging
import config_dash
import sys


def configure_log_file(log_file=config_dash.LOG_FILENAME):
    """ Module to configure the log file and the log parameters. Logs are streamed to the log file as well as the
    screen.
    Log Levels: CRITICAL:50, ERROR:40, WARNING:30, INFO:20, DEBUG:10, NOTSET	0
    """
    print("Configuring log file: {}".format(log_file))
    config_dash.LOG = logging.getLogger(config_dash.LOG_NAME)
    config_dash.LOG_LEVEL = logging.DEBUG
    config_dash.LOG.setLevel(config_dash.LOG_LEVEL)
    log_formatter = logging.Formatter('%(asctime)s - %(filename)s:%(lineno)d - %(levelname)s - %(message)s')
    # Add the handler to print to the screen
    handler1 = logging.StreamHandler(sys.stdout)
    handler1.setFormatter(log_formatter)
    config_dash.LOG.addHandler(handler1)
    # Add the handler to for the file if present
    if log_file:
        print("Started logging in the log file:{}".format(log_file))
        handler2 = logging.FileHandler(filename=log_file)
        handler2.setFormatter(log_formatter)
        config_dash.LOG.addHandler(handler2)