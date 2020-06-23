#!/cvmfs/soft.computecanada.ca/easybuild/software/2017/Core/python/3.5.4/bin/python3.5
from subprocess import run
import numpy as np
import argparse
import sys
import os
import glob
import re
import json

single_occupation_files = ["ekin.dat","pn.dat"]
single_filenames = ["Chi0.dat","Chi0Sites.dat","D.dat","DSites.dat","ekin.dat","k.dat","kSites.dat","N.dat","NSites.dat","pn.dat","sign.dat","Sz.dat","SzSites.dat"]
multiple_filenames = ["ChiFull","ChiFullSites","dgreen","green","hyb","pK","pxgreen","pxygreen","pygreen","self"]

#Parser that prints the help on error
class MyParser(argparse.ArgumentParser): 
   def error(self, message):
      sys.stderr.write('error: %s\n' % message)
      self.print_help()
      sys.exit(2)
#END

#Beginning function to handle uncomputed occupations
def find_missing_occupation(input_dir,data_dir):
	lst = np.sort(np.loadtxt(os.path.join(data_dir,"ekin.dat"))[:,0].astype(int))
	should_be = np.sort(np.loadtxt(os.path.join(data_dir,"N.dat"))[:,0].astype(int))
	for i in should_be:
		if i not in lst:
			return i
	return None
def all_occupations_in_folder(files_dir):
	data_from_autocoherence = "../"
	autocoherence_dir = "Autocoherence/"
	solver_dir = "ImpuritySolver/"
	input_dir = os.path.join(files_dir,"IN/")
	output_dir = os.path.join(files_dir,"OUT/")
	data_dir = os.path.join(files_dir,"DATA/")
	all_data_dir = os.path.join(files_dir,"../")
	index = find_missing_occupation(input_dir,data_dir)
	if index != None:
		run(["Autocoherence/run_occupation.sh", os.path.join(data_from_autocoherence,output_dir), os.path.join(data_from_autocoherence,data_dir), "params", str(index)])
		run(["sbatch","Analysis_code/actions.sh","-a","occupations","-f",os.path.basename(os.path.normpath(files_dir))])
	else:
		for file in single_occupation_files:
			data = np.loadtxt(os.path.join(data_dir,file))
			data = data[np.argsort(data[:,0])]
			np.savetxt(os.path.join(data_dir,file),data,fmt='%i %f')
			order_and_remove_occupation(files_dir)
		#We need to sort everything to be sure it's ok
	return 0
def order_and_remove_occupation(files_dir):
	autocoherence_dir = "Autocoherence/"
	solver_dir = "ImpuritySolver/"
	input_dir = os.path.join(files_dir,"IN/")
	output_dir = os.path.join(files_dir,"OUT/")
	data_dir = os.path.join(files_dir,"DATA/")
	all_data_dir = os.path.join(files_dir,"../")
	for file in single_filenames:
		data = np.loadtxt(os.path.join(data_dir,file))
		treated = data[np.unique(data[:,0],return_index = True)[1],:]
		try:
			np.savetxt(os.path.join(data_dir,file),treated,fmt="%i %f")
		except:
			np.savetxt(os.path.join(data_dir,file),treated,fmt="%i %f %f %f %f")
	return 0
def handle_occupation(files_dir):
	order_and_remove_occupation(files_dir)
	return all_occupations_in_folder(files_dir)
#END

#Beginnning functions to delete the last iteration
def find_last_results(files_dir):
	results_list = []
	for f in glob.glob(os.path.join(files_dir,"OUT/params*.meas.json")):
		results_list.append(int(re.search(r"params([0-9]+)\.meas\.json",f).group(1)))
	try:
		return(max(results_list))
	except:
		print("There is no output to delete, take a look")
		exit()
def delete_safely(file):
	try:
		os.remove(file)
	except Exception as e:
		print("Erreur au fichier " + file)
def delete_last(files_dir):
	iteration = find_last_results(files_dir)
	data_dir = os.path.join(files_dir,"DATA/")
	input_dir = os.path.join(files_dir,"IN/")
	output_dir = os.path.join(files_dir,"OUT/")
	for file in single_filenames:
		try:
			data = np.loadtxt(os.path.join(data_dir,file))
			if int(data[-1,0]) == int(iteration):
				try:
					np.savetxt(os.path.join(data_dir,file),data[:-1,:],fmt="%i %f")
				except:
						np.savetxt(os.path.join(data_dir,file),data[:-1,:],fmt="%i %f %f %f %f")
			else:
				print("Erreur au fichier " + file + " " + str(int(data[-1,0])) + " :  " + str(int(iteration)))
		except Exception as e:
			print("Erreur au fichier " + file)
	for file in multiple_filenames:
		delete_safely(os.path.join(data_dir,file + str(iteration) + ".dat"))
	delete_safely(os.path.join(input_dir,"Hyb" + str(int(iteration)+1) + ".json"))	
	delete_safely(os.path.join(input_dir,"params" + str(int(iteration)+1) + ".json"))		
	delete_safely(os.path.join(output_dir,"params" + str(iteration) + ".meas.json"))	
	return 0
#END

#Beginning functions to prepare the folder for copy to the local computer
def delete_safely(file):
    try:
        os.remove(file)
    except:
        pass

def delete_useless(folder_name):
    os.chdir(folder_name)
    print(os.getcwd())
    for f in glob.glob("slurm*"):
        os.remove(f)
    delete_safely("occupation-log.out")
    delete_safely("logfile")
    delete_safely("run.sh")
    os.chdir("OUT/")
    for f in glob.glob("config_*.json"):
        os.remove(f)

def prepare_copy(folder_name):
	delete_useless(folder_name)
#END

#Beginning functions to compute the order parameter for all iterations that were not already computed
def compute_order_parameter(folder_name):
    filename = "green"
    save_filename = folder_name + "/order_graph"
    Composante = 3
    graph = np.zeros(0)
    G = []
    offset = 1
    try:
        graph = np.load(save_filename + ".npy")
        offset+=len(graph)
    except:
        pass
    i=offset
    try:
        while(True):
            G.append([])
            f=open(folder_name + "/DATA/" + filename + str(i) + ".dat", "r")
            fl = f.readlines()
            for num, line in enumerate(fl):
                valueList = re.split(" |\n",line)[1:-1]
                G[i-offset].append(valueList)
            i+=1
            f.close()
    except FileNotFoundError as e:
        print("Fin de la lecture des fichiers, le dernier était le numéro: " + str(i-1))
        if i-1 == 0:
            print(e)
    except Exception as e:
        print(str(e))
    if len(G[0]) != 0:
        G = np.array(G[:-1]).astype(float)
        print(G.shape)
        #Ici, G[it][iwn][site]
        G = np.transpose(G,(1,2,0)) #Ici, G[iwn][site][it], mieux pour voir quelquechose
        with open(folder_name + "/IN/params1.json") as f:
            beta = json.load(f)["beta"]
        G = np.sum(G,axis=0)*2/beta
        graph = np.concatenate((graph,(G[2*(Composante+1)] - G[2*Composante])/2))
        np.save(save_filename,graph)
#END




actions_list = ["occupations","delete_last","prepare_copy","order_parameter"]
actions_functions = [handle_occupation,delete_last,prepare_copy,compute_order_parameter]

#Activate the parser to understand the input
parser = MyParser(formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument("-v","--verbose", action="store_true",help="increase output verbosity")
parser.add_argument("-a",dest="action",help="the action of the program among : \n\t" + str(actions_list),required=True)
parser.add_argument("-f",dest="folder",help="the folder you want to act on",required=True)
args = parser.parse_args()
if args.action not in actions_list:
	parser.error("-a should be in " + str(actions_list))

#Get the function we want to launch
index_in_list = actions_list.index(args.action)
#Initialize the folder structure correctly
path_to_main_dir = "../"
path_from_main_to_data = "ComputedData/"
os.chdir(os.path.join(os.path.dirname(os.path.realpath(__file__)),path_to_main_dir)) #The working directory is now the root of the repository
path_from_main_to_files = os.path.join(path_from_main_to_data + args.folder)
#Launch the action we want
actions_functions[index_in_list](path_from_main_to_files)