import string 
import sys 
import os 

res_dict = {}
def parse_res(size): 
	file= open("Res" , 'r')
	ln = string.split(file.readline(),' ')
	dict={}
	while len(ln)>1:
		ln[0]=ln[0].split('/',1)
		filename="/"+ln[0][1]
		site=ln[0][0]
		size_str=ln[1].split('/')[0]
		try:
			file_size=int(size_str)
		except ValueError:
			file_size = 0
		if file_size >= size:
			dict[file_size]=(site,filename)
		ln = ''	
		ln = file.readline().split(' ')
	file.close()
	os.system("rm -f Res")
	return dict

def crawl_site(limit , depth, size):
	global res_dict
	print "Crawling site "+sys.argv[1]+" using wget, with depth="+str(depth)+", limit="+str(limit)+"MB, and looking for files greater than "+str(size)+"KB\n"
	cmd="wget -P ./temp_abget32dn -r --no-cache --no-proxy -t 2 -Q"+str(limit)+"m -l "+str(depth)+" -p -U \"Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.7.8)\" "+sys.argv[1]+" 2>&1 | grep save | awk '{print $5 $7}' > Res;"
	cmd2="cat Res | sed 's/`.\/temp_abget32dn\///g' | sed \"s/'/ /g\" | sed 's/\[//g' | sed 's/\]//g' | awk '{print $1,$2 }'>Res"
	print "Running wget... Please wait"
	os.system(cmd)
	print "wget finished\n"
	os.system(cmd2)
	res_dict = parse_res(int(size))

def print_res(size):
	global res_dict
	file = open('Crawl_Results','a')
	keys=res_dict.keys()
	keys.sort()
	keys.reverse()
	for key in keys:
		file_size=key
		(url,filename)=res_dict[key]
		print url,filename,file_size
		if file_size>=size:
			file.write(url+" "+filename+" "+str(file_size)+"\n")
	file.close()


def check_res(size):
	global res_dict
	keys=res_dict.keys()
	keys.sort()
	keys.reverse()
	if len(keys)==0 :
		return False
	else: 
		print "Found "+str(len(keys))+" files greater than "+str(size)+"KB (host, filename, size):"
		return True
dep=0
size=0
os.system("mkdir ./temp_abget32dn")
print "\nUsage: python crawler.py host [depth] [size] [limit]\n"
if len(sys.argv)<2 :
#	print "\nUsage: python crawler.py host [depth] [size] [limit]\n"
	os.system("rm -rf ./temp_abget32dn")
	sys.exit()
elif len(sys.argv)==3:
	dep=sys.argv[2]
	size=20000
	limit=1
	crawl_site(limit,dep,size)
elif len(sys.argv)==4:
	dep=sys.argv[2]
	size=sys.argv[3]
	limit=1
	crawl_site(limit,dep,size)
elif len(sys.argv)==5:
	dep=sys.argv[2]
	size=sys.argv[3]
	limit=sys.argv[4]
	crawl_site(limit,dep,size)
else:
	dep=2
	size=20000
	limit=1
	crawl_site(limit,dep,size)

if check_res(int(size))==True :
	print_res(int(size))
else :
	print "No suitable files found (>"+str(size)+"KB), for depth="+str(dep)+" and limit="+str(limit)+"MB"
	print "Please try again with greater depth, greater limit or smaller file size"
#i=0
#while i<5 and check_res(int(size))==False:
#	i=i+1
#	dep= int(dep)+1
#	crawl_site( limit , dep, size)

os.system("rm -rf ./temp_abget32dn")
#print_res(int(size))

