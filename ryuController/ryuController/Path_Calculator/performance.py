import time


class Performance:
    """
    Class to record time performance
    """

    def __init__(self, filename):
        self.start = None
        self.end = None
        self.filename = filename
        self.count = 0

    def start_sample(self):
        self.start = time.time()

    def end_sample(self, edges):
        self.end = time.time()
        # write to file
        dif = self.end - self.start
        with open(self.filename, "a") as file_object:
            # Append 'hello' at the end of file
            file_object.write(str(self.count) + ',' +
                              str(edges) + ',' + str(dif) + '\n')

        self.count += 1
