import os
import json
import multiprocessing as mp
import subprocess
import random

relative_path = "../"
relative_path = '/'.join(__file__.split('/')[:-1]) + '/' + relative_path
project_path = os.path.abspath(relative_path) + '/'

def rand_int():
	# Generating a random integer from 0 to the maximum unsigned integer in C++
	# In C++, the maximum value for an unsigned int is typically 2^32 - 1
	max_unsigned_int_cpp = 2**32 - 1
	random_unsigned_int = random.randint(0, max_unsigned_int_cpp)
	return random_unsigned_int

def run_job(location):
	output_file = location + "sim_output.txt"
	error_file = location + "sim_errors.txt"
	cmd = [f"{location}Collider.x",location]

	with open(output_file,"a") as out, open(error_file,"a") as err:
		subprocess.run(cmd,stdout=out,stderr=err)

if __name__ == '__main__':
	#make new output folders
	curr_folder = os.getcwd() + '/'

	try:
		# os.chdir("{}ColliderSingleCore".format(curr_folder))
		subprocess.run(["make","-C",project_path+"Collider"], check=True)
	except:
		print('compilation failed')
		exit(-1)


	job_set_name = "lognorm"
	# folder_name_scheme = "T_"

	# runs_at_once = 7
	# attempts = [21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40]
	# attempts = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20] 
	attempts = [i for i in range(0,30)]
	# attempts = [90]
	# attempts_300 = [i for i in range(0,15)]
	# attempts = [1] 
	# attempts_300 = attempts

	node = 1

	N = [300,100,30]
	# N = [30]
	Temps = [3,10,30,100,300,1000]
	# Temps = [3]


	folders = []
	for attempt in attempts:
		for n in N:
			if n == 30:
				threads = 1
			elif n == 100:
				threads = 2
			else:# n == 300:
				threads = 16

			for Temp in Temps:
				# temp_attempt = attempts
				# if n == 300:
				# 	temp_attempt = attempts_300

				#load default input file
				with open(project_path+"default_files/default_input.json",'r') as fp:
					input_json = json.load(fp)
				
				# job = curr_folder + 'jobs/' + job_set_name + str(attempt) + '/'
				job = input_json["data_directory"] + 'jobs/' + job_set_name + str(attempt) + '/'\
							+ 'N_' + str(n) + '/' + 'T_' + str(Temp) + '/'

				if not os.path.exists(job):
					os.makedirs(job)
				else:
					print("Job '{}' already exists.".format(job))

				if os.path.exists(job+"timing.txt"):
					print("Sim already complete")
				else:
					#load default input file

					####################################
					######Change input values here######
					input_json['temp'] = Temp
					input_json['seed'] = rand_int()
					input_json['radiiDistribution'] = 'logNormal'
					input_json['N'] = n
					input_json['h_min'] = 0.5
					input_json['dataFormat'] = "csv"
					input_json['output_folder'] = job
					input_json['OMPthreads'] = threads
					# input_json['u_s'] = 0.5
					# input_json['u_r'] = 0.5
					input_json['note'] = "Runs lognorm for all temps and sizes "
					####################################

					with open(job + "input.json",'w') as fp:
						json.dump(input_json,fp,indent=4)


					qsubfile = ""
					qsubfile += "#!/bin/sh\n"
					# qsubfile += "#!/bin/bash\n"
					# qsubfile += "#$ -l nodes=1:ppn=1\n"
					qsubfile += "#$ -S /bin/sh\n"
					# qsubfile += "#$ -S /bin/bash\n"
					qsubfile += "#$ -q lazzati.q\n"
					qsubfile += f"#$ -N a{attempt}-N{n}-T{Temp}\n"
					qsubfile += "#$ -cwd\n"
					qsubfile += "#$ -m e\n"
					# qsubfile += "#$ -l exclusive\n"
					qsubfile += f"#$ -pe orte {threads}\n"
					qsubfile += "#$ -M kolanzl@oregonstate.edu\n"
					qsubfile += "#$ -o sim_out.log\n"
					qsubfile += "#$ -e sim_err.log\n\n"

					qsubfile += f"export OMP_NUM_THREADS={threads}\n"
					qsubfile += "module load default-environment\n"
					qsubfile += "module unload gcc/5.1.0\n"
					qsubfile += "module load gcc/12.2.0\n"
					
					qsubfile += f"{job}Collider.x {job}\n"


					
					with open(job+"qsub.bash",'w') as sfp:
						sfp.write(qsubfile)

					#add run script and executable to folders
					os.system(f"cp {project_path}Collider/Collider.x {job}Collider.x")
					os.system(f"cp {project_path}Collider/Collider.cpp {job}Collider.cpp")
					os.system(f"cp {project_path}Collider/ball_group.hpp {job}ball_group.hpp")

					folders.append(job)


print(folders)
cwd = os.getcwd()
for folder in folders:
	os.chdir(folder)
	os.system('qsub qsub.bash')
os.chdir(cwd)







	
