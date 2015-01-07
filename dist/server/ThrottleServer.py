from mod_python import apache
import time
import math

"""
Apache2 Configuration:
http://www.cyberciti.biz/faq/ubuntu-mod_python-apache-tutorial/

"""
test = 1
start_time = None
transfer_frequency = 30
log_file = "/var/www/py/speed_log.txt"
# shmem_file = "/tmp/httpthrottle.run"
# shmem_fd = open(shmem_file, "")


def get_next_rate():
    """ Return the bitrates in bytes per second"""
    return 100


def handler(req):
    req.log_error('HTTP Throttle handler')
    req.content_type = 'text/html'
    req.send_http_header()
    if req.filename.endswith("m4s"):
        try:
            data_file = open(req.filename, "rb")
        except IOError:
            return apache.HTTP_NOT_FOUND
        send_file_at_log_speed_V1(req, data_file)
    else:
        req.write('<html><head><title>Testing mod_python</title></head><body>')
        global test
        req.write(str(test) + 'Hello World!!!' + req.filename)
        test += 1
        req.write('</body></html>')
    return apache.OK


#e input from a logfile to throttle bandwidth..
# One log line each second, given in kbit/sec.
#


def send_file_at_log_speed_V1(req, data_file):
    """
    :param req: mp_request object
    :param data_file: file to be written to HTTP connection
    :return: HTTP Code
    """
    global start_time
    if not start_time:
        start_time = time.time()
    while True:
        t = time.time() - start_time
        speed = get_next_rate()
        prev_time = time.time()
        transfers = int(math.floor(t + 1.0) * transfer_frequency - round(t * transfer_frequency))
        for i in range(transfers):
            time.sleep(1.0/transfer_frequency)
            cur_time = time.time()
            tdiff = cur_time - prev_time
            prev_time = cur_time
            if speed != 0:
                data = data_file.read(int(speed*tdiff))
                if not data:
                    break
                req.write(data)
                req.flush()
        if transfers == 0:
            time.sleep(0.5/transfer_frequency)


# def send_file_at_log_speed(req):
#     """MOdule to write the data based on the log bitrates  """
#     while True:
#         #t = time.time() - start_time
#         transfers = 5
#         #transfers = int(math.floor(t + 1.0) * transfer_frequency - round(t * transfer_frequency))
#         ##transfers = 5
#         #for i in range(prev_pos, int(t)):
#         #    log_line = log_handle.readline()
#         #    if log_line is None or not log_line.strip().isdigit():
#         #           req.pass_on()
#         #           return
#         #prev_pos = int(t)
#         speed = get_next_rate()
#         prev_time = time.time()
#         for i in range(transfers):
#             time.sleep(1.0/transfer_frequency)
#             #time.sleep(10)
#             #read_globals()
#             cur_time = time.time()
#             tdiff = cur_time - prev_time
#             prev_time = cur_time
#             if speed != 0:
#                 data = data_file.read(int(speed*tdiff))
#                 #data = data_file.read(1024)
#                 if not data:
#                     break
#                 req.write(data)
#                 req.flush()
#         if transfers == 0:
#             time.sleep(0.5/transfer_frequency)
#         log_handle.close()
#
# #def read_globals():
# #    global start_time
# #    global active_conns
# #    global shmem_fd
# #    #shmem_fd.seek(0)
# #    #start_time = float(shmem_fd.readline())
#     #active_conns = int(shmem_fd.readline())
