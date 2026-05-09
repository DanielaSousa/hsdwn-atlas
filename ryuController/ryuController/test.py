
import pickle


#import ryuController.exp_msg as exp
#from utils import *
#from routing import *
#from paths import PathState
import networkx as nx


buf = b'\x01\x00\x00\x00\x02\x00\x00\x00\x16\x89\x02\x00\x00\x00\x00\x00\xc3\x03?\xe3\xc2\xd6S\xc0`\xcd7\x82~8-@ELu\xfb\xc2rJA\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x01\x02\x01\n\x01\x00\x00\x00\x03\x02\x01\n\x01V\x00\x00P\x16(\xff\x03'


# open a file, where you stored the pickled data
file = open('/home/phd/Documents/PhD/ns-3-dev/scenario1.pickle', 'rb')

# dump information to that file
rm = pickle.load(file)

# close the file
file.close()

nx.draw(rm.net.G)

#print(exp.parse_message(buf, 1, rm.net))

