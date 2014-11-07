import time


class StopWatch():
    """ Implements a stop watch function
        Modified from http://code.activestate.com/recipes/124894-stopwatch-in-tkinter/
    """
    def __init__(self):
        self._start = 0.0
        self._elapsedtime = 0.0
        self._running = 0

    def start(self):
        """ Start the stopwatch, ignore if running. """
        if not self._running:            
            self._start = time.time() - self._elapsedtime
            self._running = 1        
    
    def pause(self):
        """ Stop the stopwatch, ignore if stopped. """
        if self._running:
            self._elapsedtime = time.time() - self._start
            self._running = 0
    
    def reset(self):
        """ Reset the stopwatch. """
        self._start = time.time()         
        self._elapsedtime = 0.0

    def time(self):
        if self.running:
            self._elapsedtime = time.time() - self._start
        return int(self._elapsedtime)


