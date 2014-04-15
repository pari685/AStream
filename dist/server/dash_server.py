#!/usr/bin/env python
"""A simple HTTP server

To start the server:
    python delayserver.py

Ipython Run:
    import BaseHTTPServer
    from delayserver import MyHTTPRequestHandler
    http = BaseHTTPServer.HTTPServer( ('localhost', 8000),
        MyHTTPRequestHandler)
    http.serve_forever()

To test with browser : 'http://198.248.242.16:8005/mpd/index.html')

To retriev from Python2.7 Shell:
    import urllib2
     urllib2.urlopen(
            "http://198.248.242.16:8006/mpd/x4ukwHdACDw.mpd").read()
To Test from client:
  from urllib2 import urlopen
  data = urlopen("http://198.248.242.16:8006/x4ukwHdACDw/video/1/seg-0001.m4f"))

"""
import time
import BaseHTTPServer
import sys
import os
from argparse import ArgumentParser
from collections import defaultdict
#sys.path.append('..')

# Default values
DEFAULT_HOSTNAME = '198.248.242.16'
DEFAULT_PORT = 8006
DEFAULT_SLOW_RATE = 0

BLOCK_SIZE = 1024

# Values set by the option parser
PORT = DEFAULT_PORT
HOSTNAME = DEFAULT_HOSTNAME
HTTP_VERSION = "HTTP/1.1"
# 10 kbps when size is in bytes
SLOW_RATE = DEFAULT_SLOW_RATE

HTML_PAGES = ['index.html']
MPD_FILES = ['mpd/index.html', 'mpd/x4ukwHdACDw.mpd']

# dict that holds the current active sessions
ACTIVE_DICT = defaultdict(list)

# DELAY Parameters
# Number of the segement to insert delay
#COUNT = 3
SLOW_RATE = 5
COUNT = 3
def get_count():
    """ Module that returns a random value """
    for i in range(1, 1000):
        yield COUNT*i

COUNT_ITER = get_count()
DELAY_VALUES = dict()

class MyHTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    "HTTPHandler to serve the DASH video"
    def do_GET(self):
        "Function to handle the get message"
        request = self.path.strip("/").split('?')[0]
        # connection_id = client IP, dirname of file
        connection_id = (self.client_address[0],
                            os.path.dirname(self.path))
        shutdown = False
        kwargs = {}
        if request in HTML_PAGES:
            print "Request HTML %s" % (request)
            duration = normal_write(self.wfile,
                    request, **kwargs)
        elif request in MPD_FILES:
            print "Request for MPD %s" % (request)
            duration = normal_write(self.wfile,
                    request, **kwargs)
            # assuming that the new session always
            # starts with the download of the MPD file
            # Making sure that older sessions are not
            # in the ACTIVE_DICT
            if connection_id in ACTIVE_DICT:
                del(ACTIVE_DICT[connection_id])
                del(DELAY_VALUES[connection_id])
            else:
                DELAY_VALUES[connection_id] = get_count()
        elif request.split('.')[-1] in ['m4f', 'mp4']:
            print "Request for DASH Media %s" % (request)
            ACTIVE_DICT[connection_id].append(os.path.basename(request))

            if len(ACTIVE_DICT[connection_id])%3 == 0:
                duration = slow_write(output=self.wfile,
                        request=request, rate=SLOW_RATE)
                print 'Slow: Request took %f seconds' % (duration)
            else:
                duration = normal_write(self.wfile,
                        request, **kwargs)
                print 'Normal: Request took %f seconds' % (duration)
        else:
            self.send_error(404)
            return
        self.send_response( 200 )
        self.send_header('ContentType', 'text/plain;charset=utf-8')
        self.send_header('Content-Length', str(os.path.getsize(request)))
        self.send_header('Pragma', 'no-cache' )
        self.end_headers()
        if shutdown:
            self.server.shutdown()

def normal_write(output, request):
    "Function to write the video onto output stream"
    with open(request, 'r') as request_file:
        start_time = time.time()
        output.write(request_file.read())
        now = time.time()
        output.flush()
    return now - start_time

def slow_write(output, request, rate=None):
    """Function to write the video onto output stream with interruptions in
    the stream
    """
    print "Slow write of %s" % request
    with open(request, 'r') as request_file:
        start_time = time.time()
        data = request_file.read(BLOCK_SIZE)
        output.write(data)
        last_send = time.time()
        current_stream = len(data)
        while (data != ''):
            if rate:
                print "Slow write of %s at rate %f" % (request, rate)
                if curr_send_rate(BLOCK_SIZE, last_send - time.time()) >  rate:
                    continue
            output.write(data)
            last_send = time.time()
            current_stream += len(data)
            data = request_file.read(BLOCK_SIZE)
        now = time.time()
        output.flush()
    print 'Served %d bytes of file: %s in %f seconds' % (
                    current_stream, request, now - start_time)
    return now - start_time

def curr_send_rate(data_size, time_to_send_data):
    """ Method to calculate the current rate at which the data 
        is being sent. Data_size in byes
        The return value is in kbps
    """
    while True:
        try:
            rate = data_size * 8 / (time_to_send_data) / 1000
            break
        except ZeroDivisionError:
            continue
    return rate

def start_server():
    """ Module to start the server"""
    http_server = BaseHTTPServer.HTTPServer((HOSTNAME, PORT),
                                            MyHTTPRequestHandler)
    print " ".join(("Listening on ", HOSTNAME, " at Port ",
        str(PORT), " - press ctrl-c to stop"))
    # Use this Version of HTTP Protocol
    http_server.protocol_version = HTTP_VERSION
    http_server.serve_forever()

def create_arguments(parser):
    """ Adding arguments to the parser"""
    parser.add_argument('-p', '--PORT', type=int,
            help=("Port Number to run the server. Default = %d" % DEFAULT_PORT),
            default=DEFAULT_PORT)
    parser.add_argument('-s', '--HOSTNAME',
            help=("Hostname of the server. Default = %s" % DEFAULT_HOSTNAME),
            default=DEFAULT_HOSTNAME)
    parser.add_argument('-d', '--SLOW_RATE', type=float,
            help=("Delay value for the server in msec. Default = %f"
                % DEFAULT_SLOW_RATE), default=DEFAULT_SLOW_RATE)

def update_config(args):
    """ Module to update the config values with the a
    arguments """
    globals().update(vars(args))
    return

def main(argv=None):
    "Program wrapper"
    if not argv:
        argv = sys.argv[:1]
    parser = ArgumentParser(description='Process server parameters')
    create_arguments(parser)
    args = parser.parse_args()
    update_config(args)
    start_server()

if __name__ == "__main__":
    sys.exit(main())

