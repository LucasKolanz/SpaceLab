import os
import json
import multiprocessing as mp
import subprocess

def run_job(location,num_balls):
	# cmd = ["python3", "{}run_multicore_sim.py".format(location), location, str(num_balls)]
	cmd = ["python3", "{}run_multicore_sim2.py".format(location), location, str(num_balls)]
	# print(cmd)
	subprocess.run(cmd)

if __name__ == '__main__':
	#make new output folders
	curr_folder = os.getcwd() + '/'

	try:
		# os.chdir("{}ColliderSingleCore".format(curr_folder))
		# subprocess.run(["make","-C","ColliderMultiCore"], check=True)
		subprocess.run(["make","-C","ColliderMultiCore2electricBugaloo"], check=True)
	except:
		print('compilation failed')
		exit(-1)
		
	# job_set_name = "multiCoreCollideTest"
	job_set_name = "multiCoreCollide2Test"
	# job_set_name = "multiCoreTest"
	# job_set_name = "multiCore2Test"
	# folder_name_scheme = "T_"

	runs_at_once = 1
	# attempts = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20] 
	attempts = [30] 
	N = [30]
	Temps = [100]
	folders = []
	for attempt in attempts:
		for n in N:
			for Temp in Temps:
				job = curr_folder + 'jobs/' + job_set_name + str(attempt) + '/'
				# job = curr_folder + 'jobs/' + job_set_name + str(attempt) + '/'
							# + 'N_' + str(n) + '/' + 'T_' + str(Temp) + '/'
				if not os.path.exists(job):
					os.makedirs(job)
				else:
					print("Job '{}' already exists.".format(job))

				# os.system("cp {}/jobs/startingAgg/* {}".format(curr_folder,job))

				#load default input file
				with open(curr_folder+"default_files/default_input.json",'r') as fp:
					input_json = json.load(fp)

				####################################
				######Change input values here######
				input_json['temp'] = Temp
				input_json['seed'] = 101
				input_json['radiiDistribution'] = 'constant'
				# input_json['kConsts'] = 3e3
				input_json['h_min'] = 0.5
				# input_json['u_s'] = 0.5
				# input_json['u_r'] = 0.5
				# input_json['projectileName'] = "299_2_R4e-05_v4e-01_cor0.63_mu0.1_rho2.25_k4e+00_Ha5e-12_dt5e-10_"
				# input_json['targetName'] = "299_2_R4e-05_v4e-01_cor0.63_mu0.1_rho2.25_k4e+00_Ha5e-12_dt5e-10_"
				input_json['note'] = "Collapse manually, serial."
				####################################

				with open(job + "input.json",'w') as fp:
					json.dump(input_json,fp,indent=4)

				#add run script and executable to folders
				# os.system("cp ../default_files/run_multicore_sim.py {}run_multicore_sim.py".format(job))
				# os.system("cp ColliderMultiCore/ColliderMultiCore.x {}ColliderMultiCore.x".format(job))
				os.system("cp ../default_files/run_multicore_sim2.py {}run_multicore_sim2.py".format(job))
				os.system("cp ColliderMultiCore2electricBugaloo/ColliderMultiCore2.x {}ColliderMultiCore2.x".format(job))
				folders.append(job)
	# print(folders)
	if len(N) != len(folders):
		N = [str(N[0]) for i in range(len(folders))]

	inputs = list(zip(folders,N))
	print(inputs)

	for i in range(0,len(folders),runs_at_once):
		with mp.Pool(processes=runs_at_once) as pool:
			pool.starmap(run_job,inputs[i:i+runs_at_once]) 


	
