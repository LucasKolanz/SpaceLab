import numpy as np
import matplotlib.pyplot as plt
import sys
import json
import os
import csv

relative_path = ""
relative_path = '/'.join(__file__.split('/')[:-1]) + '/' + relative_path
project_path = os.path.abspath(relative_path) + '/'

sys.path.append(project_path+"utilities/")
# sys.path.append("/home/kolanzl/Desktop/SpaceLab/")
import utils as u
# import utils_old as u


relative_path = ""
relative_path = '/'.join(__file__.split('/')[:-1]) + '/' + relative_path
project_path = os.path.abspath(relative_path) + '/'

'''
porosity definitions 1-3 from:
MODELING POROUS DUST GRAINS WITH BALLISTIC AGGREGATES. I.
GEOMETRY AND OPTICAL PROPERTIES
'''

#this function taken from 
#https://scipython.com/book/chapter-6-numpy/problems/p65/the-moment-of-inertia-tensor/
def translate_to_cofm(mass, data):
	# Position of centre of mass in original coordinates
	cofm = sum(mass * data) / (mass*data.shape[0])
	# Transform to CofM coordinates and return
	data -= cofm
	return data

#this function taken from 
#https://scipython.com/book/chapter-6-numpy/problems/p65/the-moment-of-inertia-tensor/
def get_inertia_matrix(mass, data):
	# Moment of intertia tensor
	
	#should in general translate to center of mass
	#but data is already there
	# data = translate_to_cofm(mass, data)

	x, y, z = data.T

	Ixx = np.sum(mass * (y**2 + z**2))
	Iyy = np.sum(mass * (x**2 + z**2))
	Izz = np.sum(mass * (x**2 + y**2))
	Ixy = -np.sum(mass * x * y)
	Iyz = -np.sum(mass * y * z)
	Ixz = -np.sum(mass * x * z)
	I = np.array([[Ixx, Ixy, Ixz], [Ixy, Iyy, Iyz], [Ixz, Iyz, Izz]])
	# print(I)
	return I

#this function taken from 
#https://scipython.com/book/chapter-6-numpy/problems/p65/the-moment-of-inertia-tensor/
def get_principal_moi(mass,data):
	I = get_inertia_matrix(mass,data)
	Ip = np.linalg.eigvals(I)
	# Sort and convert principal moments of inertia to SI (kg.m2)
	Ip.sort()
	return Ip

def porosity_measure1(data_folder,data_index=-1):
	data,radius,mass,moi = u.get_data(data_folder,data_index)
	if data is None:
		return np.nan
	num_balls = data.shape[0]

	effective_radius = np.power(np.sum(np.power(radius,3)),1/3) 


	# effective_radius = radius*np.power(num_balls,1/3) 
		
	principal_moi = get_principal_moi(np.mean(mass),data)
	# principal_moi = get_principal_moi(mass,data)
	
	# print(principal_moi)
	# print(mass)
	# print(np.mean(mass))
	# exit(0)
	
	alphai = principal_moi/(0.4*np.sum(mass)*effective_radius**2)
	# alphai = principal_moi/(0.4*num_balls*mass*effective_radius**2)
	
	a = effective_radius * np.sqrt(alphai[1] + alphai[2] - alphai[0])
	b = effective_radius * np.sqrt(alphai[2] + alphai[0] - alphai[1])
	c = effective_radius * np.sqrt(alphai[0] + alphai[1] - alphai[2])
	
	# Rabc = np.power(a*b*c,1/3)
	porosity = 1-(effective_radius**3/(a*b*c))

	return porosity

def porosity_measure2(data_folder,data_index=-1):
	data,radius,mass,moi = u.get_data(data_folder,data_index)
	if data is None:
		return np.nan
	num_balls = data.shape[0]

	effective_radius = np.power(np.sum(np.power(radius,3)),1/3)  
	# effective_radius = radius*np.power(num_balls,1/3) 
		
	principal_moi = get_principal_moi(np.mean(mass),data)
	# principal_moi = get_principal_moi(mass,data)

	alphai = principal_moi/(0.4*np.sum(mass)*effective_radius**2)
	# alphai = principal_moi/(0.4*num_balls*mass*effective_radius**2)

	RKBM = np.sqrt(np.sum(alphai)/3) * effective_radius

	porosity = 1-np.power((effective_radius/RKBM),3)
	return porosity

# def dist(i,j,)

def number_of_contacts(data_folder,data_index=-1):
	data,radius,mass,moi = u.get_data(data_folder,data_index)
	if data is None:
		return np.nan
	data = np.array(data)
	num_balls = data.shape[0]

	contacts = np.zeros((num_balls,num_balls),dtype=int)
	dist = lambda i,j: np.sqrt((data[i][0]-data[j][0])**2 + (data[i][1]-data[j][1])**2 + \
			(data[i][2]-data[j][2])**2)

	for i in range(num_balls):
		for j in range(num_balls):
			if i != j:
				contacts[i,j] = (dist(i,j) <= radius[i]+radius[j])
	
	return np.mean(np.sum(contacts,axis=1))

if __name__ == '__main__':
	with open(project_path+"default_files/default_input.json",'r') as fp:
		input_json = json.load(fp)
	
	# job = curr_folder + 'jobs/' + job_set_name + str(attempt) + '/'
	path = input_json["data_directory"]
	# data_prefolder = path + 'jobs/calibrateTest'
	# data_prefolder = path + 'jobs/mu_max'
	# data_prefolder = path + 'jobs/h_max'
	# data_prefolder = path + 'jobs/lognorm'
	# data_prefolder = path + 'jobs/tempVarianceRand_attempt'
	data_prefolder = path + 'jobsCosine/lognorm'
	data_prefolder = path + 'jobsNovus/const'
	dataset_name = data_prefolder.split("/")[-1]

	sav = path+'data/{}_averageData.csv'.format(dataset_name)
	# figure_folder = 'figuresCompare/'
	figure_folder = path+'data/figuresCosine/'


	# temps = [10]
	temps = [3,10,30,100,300,1000]
	# temps = [3,10,30]
	Nums = [30,100,300]
	# Nums = [30,100]
	
	
	attempts = [i for i in range(30)]
	attempts300 = attempts
	# attempts300 = [i for i in range(9)]
	# data = [] 

	porositiesabcavg = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	porositiesKBMavg = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	KBM_numruns = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	ABC_numruns = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	porositiesabcstd = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	porositiesKBMstd = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	contactsavg = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	contactsstd = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	contacts_numruns = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	FD_dataavg = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	FD_datastd = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	FD_numruns = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	yerr_abc = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	yerr_KBM = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	yerr_FD = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)
	yerr_ca = np.full(shape=(len(Nums),len(temps)),fill_value=np.nan,dtype=np.float64)


	properties = 12 #number of columns to save for every Num

	# porositiesabcavg[:] = np.nan
	# porositiesKBMavg[:] = np.nan
	# porositiesabcstd[:] = np.nan
	# porositiesKBMstd[:] = np.nan
	# contactsavg[:] = np.nan
	# contactsstd[:] = np.nan
	# FD_dataavg[:] = np.nan
	# FD_datastd[:] = np.nan
	# yerr_abc[:] = np.nan
	# yerr_KBM[:] = np.nan
	# yerr_FD[:] = np.nan
	# yerr_ca[:] = np.nan

	new_data = True
	save_data = True
	show_plots = True
	show_FD_plots = False
	overwrite_octree_data = True
	find_stats = False
	show_stat_plots = False

		# pass

	if new_data:
		std_dev = []
		std_err = []
		# attempts = [attempts[3]]
		num_attempts = np.max([len(attempts),len(attempts300)])
		porositiesabc = np.full(shape=(len(Nums),len(temps),num_attempts),fill_value=np.nan,dtype=np.float64)
		porositiesKBM = np.full(shape=(len(Nums),len(temps),num_attempts),fill_value=np.nan,dtype=np.float64) 
		FD_data = np.full(shape=(len(Nums),len(temps),num_attempts),fill_value=np.nan,dtype=np.float64)
		contacts = np.full(shape=(len(Nums),len(temps),num_attempts),fill_value=np.nan,dtype=float)
		# porositiesabc[:] = np.nan
		# porositiesKBM[:] = np.nan
		# FD_data[:] = np.nan
		# contacts[:] = np.nan
		for i,temp in enumerate(temps):
			for n,N in enumerate(Nums):
				# if N == 300:
				# 	a = attempts300
				# else:
				# 	a = attempts
				a = attempts
				for j,attempt in enumerate(a):
					# temp = 100
					# N = 300
					# attempt = 12
					# data_folder = data_prefolder + str(attempt) + '/'
					data_folder = data_prefolder + str(attempt) + '/' + 'N_' + str(N) + '/T_' + str(temp) + '/'
					count = 0
					if os.path.exists(data_folder+"timing.txt"):
						porositiesabc[n,i,j] = porosity_measure1(data_folder,N-1)
						porositiesKBM[n,i,j] = porosity_measure2(data_folder,N-1)
						contacts[n,i,j] = number_of_contacts(data_folder,N-1)

						if not np.isnan(porositiesabc[n,i,j]):
							o3dv = u.o3doctree(data_folder,overwrite_data=overwrite_octree_data,index=N-1,Temp=temp)
							o3dv.make_tree()
							FD_data[n,i,j] = o3dv.calc_fractal_dimension(show_graph=show_FD_plots)
						# print("FD :\t{}".format(FD_data[n,i,j]))
						# print("abc:\t{}".format(porositiesabc[n,i,j]))
						# print("KBM:\t{}".format(porositiesKBM[n,i,j]))
						# print("con:\t{}".format(contacts[n,i,j]))

					# else:
					# 	porositiesabc[n,i,j] = np.nan
					# 	porositiesKBM[n,i,j] = np.nan
					# 	FD_data[n,i,j] = np.nan
					# exit(0)
		# porositiesabc = np.array(porositiesabc,dtype=np.float64)
		# porositiesKBM = np.array(porositiesKBM,dtype=np.float64)
		# print(porositiesabc.shape)
		# print(porositiesabc.shape)

		# for i in range(len(attempts)):
		# 	pors = np.array([porositiesabc[:,i],porositiesKBM[:,i],np.array(porositiesabc[:,i])/np.array(porositiesKBM[:,i])])
		# 	plt.plot(temps,pors.T)
		# 	plt.title('Porosity run {}'.format(i))
		# 	plt.xlabel('Temperature in K')
		# 	plt.ylabel('Porosity')
		# 	plt.legend(['Rabc','RKBM','Rabc/RKBM'])
		# 	plt.xscale('log')
		# 	plt.show()

		###### Print stat values for given N, T ######
		if find_stats:
			for T_i in range(len(temps)):
				for N_i in range(len(Nums)):
					# T_i = 0
					# N_i = 0
					nonNans = np.count_nonzero(~np.isnan(FD_data[N_i,T_i]))
					print("###########################################")
					if nonNans == 0:
						print("All nan values For T={}K, N={} balls".format(\
							temps[T_i],Nums[N_i]))
					else:	
						print("For T={}K, N={} balls, {} trials".format(\
							temps[T_i],Nums[N_i],nonNans))
						print("Average\tFD: {}".format(np.nanmean(FD_data[N_i,T_i,:])))
						print("std\tFD: {}".format(np.nanstd(FD_data[N_i,T_i,:])))
						print("range\tFD: {}".format(np.nanmax(FD_data[N_i,T_i,:])-np.nanmin(FD_data[N_i,T_i,:])))
						print()
						print("Average\tabc: {}".format(np.nanmean(porositiesabc[N_i,T_i,:])))
						print("std\tabc: {}".format(np.nanstd(porositiesabc[N_i,T_i,:])))
						print("range\tabc: {}".format(np.nanmax(porositiesabc[N_i,T_i,:])-np.nanmin(porositiesabc[N_i,T_i,:])))
						print()
						print("Average\tKBM: {}".format(np.nanmean(porositiesKBM[N_i,T_i,:])))
						print("std\tKBM: {}".format(np.nanstd(porositiesKBM[N_i,T_i,:])))
						print("range\tKBM: {}".format(np.nanmax(porositiesKBM[N_i,T_i,:])-np.nanmin(porositiesKBM[N_i,T_i,:])))
						print()
						print("Average\tcontacts: {}".format(np.nanmean(contacts[N_i,T_i,:])))
						print("std\tcontacts: {}".format(np.nanstd(contacts[N_i,T_i,:])))
						print("range\tcontacts: {}".format(np.nanmax(contacts[N_i,T_i,:])-np.nanmin(contacts[N_i,T_i,:])))
						print("###########################################")

							#Plot avg FD vs number of pts
						FD_avgsums = [np.mean(FD_data[N_i,T_i,:i]) for i in range(nonNans)] 
						fig,ax = plt.subplots()
						ax.plot(range(nonNans),FD_avgsums,\
									label="t={}K, n={}".format(temps[T_i],Nums[N_i]))

						ax.set_xlabel('Number of sims')
						ax.set_title('FD average vs number of sims')
						ax.set_ylabel('average FD')
						ax.axhline(FD_avgsums[-1],label='Overall avg')

						fig.legend()
						plt.savefig("{}meanFDsum.png".format(figure_folder))
						if show_stat_plots:
							plt.show()
						plt.close("all")

							#Plot avg abc vs number of pts
						porositiesabc_avgsums = [np.mean(porositiesabc[N_i,T_i,:i]) for i in range(nonNans)] 
						fig,ax = plt.subplots()
						ax.plot(range(nonNans),porositiesabc_avgsums,\
									label="t={}K, n={}".format(temps[T_i],Nums[N_i]))

						ax.set_xlabel('Number of sims')
						ax.set_title('abc average vs number of sims')
						ax.set_ylabel('average abc')
						ax.axhline(porositiesabc_avgsums[-1],label='Overall avg')

						fig.legend()
						plt.savefig("{}meanporositiesabcsum.png".format(figure_folder))
						if show_stat_plots:
							plt.show()
						plt.close("all")

							#Plot avg FD vs number of KBM
						porositiesKBM_avgsums = [np.mean(porositiesKBM[N_i,T_i,:i]) for i in range(nonNans)] 
						fig,ax = plt.subplots()
						ax.plot(range(nonNans),porositiesKBM_avgsums,\
									label="t={}K, n={}".format(temps[T_i],Nums[N_i]))

						ax.set_xlabel('Number of sims')
						ax.set_title('KBM average vs number of sims')
						ax.set_ylabel('average KBM')
						ax.axhline(porositiesKBM_avgsums[-1],label='Overall avg')

						fig.legend()
						plt.savefig("{}meanporositiesKBMsum.png".format(figure_folder))
						if show_stat_plots:
							plt.show()
						plt.close("all")

						contacts_avgsums = [np.mean(contacts[N_i,T_i,:i]) for i in range(nonNans)] 
						fig,ax = plt.subplots()
						ax.plot(range(nonNans),contacts_avgsums,\
									label="t={}K, n={}".format(temps[T_i],Nums[N_i]))

						ax.set_xlabel('Number of sims')
						ax.set_title(' average vs number of sims')
						ax.set_ylabel('average contacts')
						ax.axhline(contacts_avgsums[-1],label='Overall avg')

						fig.legend()
						plt.savefig("{}meanpcontactssum.png".format(figure_folder))
						if show_stat_plots:
							plt.show()
						plt.close("all")
		########## End printing stat values ##########


		for i,N in enumerate(Nums):
			if N == 300:
				a = attempts300
			else:
				a = attempts
			# porositiesabcavg.append(np.average(porositiesabc[i],axis=1))
			# porositiesKBMavg.append(np.average(porositiesKBM[i],axis=1))
			# FD_dataavg.append(np.average(FD_data[i],axis=1))
			# print('N={}\n{}'.format(N,np.nanmean(porositiesabc[i],axis=1)))
			# print(porositiesabc[i])
			# print(notnan)
			# print(porositiesabc[i,notnan])

			# print(porositiesabcavg[i].shape)
			notnan = ~np.isnan(FD_data[i])
			# print(notnan)
			if np.sum(notnan) == 0:
				continue
			# print(porositiesabcavg[i,notnan[0]].shape)
			# print(np.nanmean(porositiesabc[i],axis=1).shape)
			# print()
			# print(porositiesabcavg.shape)
			# print(np.nanmean(porositiesabc[i],axis=1))
			# print(porositiesabc[i])
			porositiesabcavg[i] = np.nanmean(porositiesabc[i],axis=1)
			# exit(0)
			porositiesKBMavg[i] = np.nanmean(porositiesKBM[i],axis=1)
			FD_dataavg[i] = np.nanmean(FD_data[i],axis=1)
			contactsavg[i] = np.nanmean(contacts[i],axis=1)
			porositiesabcstd[i] = np.nanstd(porositiesabc[i],axis=1)
			porositiesKBMstd[i] = np.nanstd(porositiesKBM[i],axis=1)
			FD_datastd[i] = np.nanstd(FD_data[i],axis=1)
			contactsstd[i] = np.nanstd(contacts[i],axis=1)
			# print(porositiesabcavg[i])
			# print(porositiesKBMavg[i])

			# plotme = np.array([porositiesabcavg,porositiesKBMavg])
			# print(porositiesabcstd[i]/np.sqrt(len(a)))
			# print(yerr_abc[i,notnan[0]])
			ABC_numruns[i] = np.count_nonzero(~np.isnan(porositiesabc[i]),axis=1)
			yerr_abc[i] = porositiesabcstd[i]/np.sqrt(ABC_numruns[i])
			
			KBM_numruns[i] = np.count_nonzero(~np.isnan(porositiesKBM[i]),axis=1)
			yerr_KBM[i] = porositiesKBMstd[i]/np.sqrt(KBM_numruns[i])

			FD_numruns[i] = np.count_nonzero(~np.isnan(FD_data[i]),axis=1)
			yerr_FD[i] = FD_datastd[i]/np.sqrt(FD_numruns[i])
			
			contacts_numruns[i] = np.count_nonzero(~np.isnan(contacts[i]),axis=1)
			yerr_ca[i] = contactsstd[i]/np.sqrt(contacts_numruns[i])
			# print(yerr[0])



		#make table
		# fig, ax = plt.subplots()
		# fig.patch.set_visible(False)


		headers = ['Temperature']
		data = np.full(shape=(len(Nums)*properties,len(porositiesabcavg[0])),fill_value=np.nan,dtype=np.float64)
		for i,t in enumerate(temps):
			data[0,i] = t

		for i,N in enumerate(Nums):
			headers.append('N={} abc porosity'.format(N))
			data[i*properties,:] = porositiesabcavg[i]
			headers.append('N={} abc std err'.format(N)) 
			data[i*properties+1,:] = yerr_abc[i]
			headers.append(f'N={N} abc number of runs')
			data[i*properties+2,:] = ABC_numruns[i]

			headers.append('N={} KBM porosity'.format(N))
			data[i*properties+3,:] = porositiesKBMavg[i]
			headers.append('N={} KBM std err'.format(N))
			data[i*properties+4,:] = yerr_KBM[i]
			headers.append('N={} KBM number of runs'.format(N))
			data[i*properties+5,:] = KBM_numruns[i]

			headers.append('N={} Fractal Dimension'.format(N))
			data[i*properties+6,:] = FD_dataavg[i]
			headers.append('N={} Fractal Dimension std err'.format(N))
			data[i*properties+7,:] = yerr_FD[i]
			headers.append('N={} Fractal Dimension number of runs'.format(N))
			data[i*properties+8,:] = FD_numruns[i]

			headers.append('N={} average number of contacts'.format(N))
			data[i*properties+9,:] = contactsavg[i]
			headers.append('N={} average number of contacts std err'.format(N))
			data[i*properties+10,:] = yerr_ca[i]
			headers.append('N={} average number of contacts number of runs'.format(N))
			data[i*properties+11,:] = contacts_numruns[i]

		data = np.array(data)
		# print(data)

		# headers.append('N={} abc porositity'.format(300))
		# data.append(porositiesabc[2,:,0])
		# headers.append('N={} KBM porositity'.format(300))
		# data.append(porositiesKBM[2,:,0])
		# headers.append('N={} Fractal Dimension'.format(300))
		# data.append(FD_data[2,:,0])
		
		# with open(sav,'w') as file:
		# 	write = csv.writer(file)
		# 	write.writerow(headers)
		# 	for row in np.transpose(np.array(data)):
		# 		write.writerow(row)
		if save_data:
			np.savetxt(sav,data)
			print("Data saved to {}".format(sav))
	else:

		# headers = np.loadtxt(sav,delimiter=',',dtype=str)[0]
		data = np.loadtxt(sav,delimiter=' ',skiprows=0,dtype=np.float64)
		# print(data.shape)
		# data = np.transpose(data)
		# exit(0)

		for i,N in enumerate(Nums):
			porositiesabcavg[i] = data[i*properties,:]
			yerr_abc[i] = data[i*properties+1,:]
			ABC_numruns = data[i*properties+2,:]

			porositiesKBMavg[i] = data[i*properties+3,:]
			yerr_KBM[i] = data[i*properties+4,:]
			KBM_numruns[i] = data[i*properties+5,:]

			FD_dataavg[i] = data[i*properties+6,:]
			yerr_FD[i] = data[i*properties+7,:]
			FD_numruns[i] = data[i*properties+8,:]

			contactsavg[i] = data[i*properties+9,:]
			yerr_ca[i] = data[i*properties+10,:]
			contacts_numruns[i] = data[i*properties+11,:]
			# porositiesabcavg[i] = data[:,i*6]
			# yerr_abc[i] = data[:,i*6+1]
			# porositiesKBMavg[i] = data[:,i*6+2]
			# yerr_KBM[i] = data[:,i*6+3]
			# FD_dataavg[i] = data[:,i*6+4]
			# yerr_FD[i] = data[:,i*6+5]
		# for i,N in enumerate(Nums):
		# 	contactsavg[i] = data[6*2 + i*2,:]
		# 	yerr_ca[i] = data[6*2 + 1 + i*2,:]

			# contactsavg[i] = data[:,6*2 + i*2]
			# yerr_ca[i] = data[:,6*2 + 1 + i*2]

		# data_skip = 6
		# porositiesabcavg = np.array([data[1],data[1+data_skip],data[1+2*data_skip]])
		# yerr_abc = np.array([data[2],data[2+data_skip],data[2+2*data_skip]])
		# porositiesKBMavg = np.array([data[3],data[3+data_skip],data[3+2*data_skip]])
		# yerr_KBM = np.array([data[4],data[4+data_skip],data[4+2*data_skip]])
		# FD_dataavg = np.array([data[5],data[5+data_skip],data[5+2*data_skip]])
		# yerr_FD = np.array([data[6],data[6+data_skip],data[6+2*data_skip]])
		# contactsavg = np.array([data[3*data_skip+1],data[3*data_skip+3],data[3*data_skip+5]])
		# yerr_ca = np.array([data[3*data_skip+2],data[3*data_skip+4],data[3*data_skip+6]])

		# print(data[1])
		# exit(0)

	
	print("======================Starting figures======================")
	print("Data has {} nan values".format(np.count_nonzero(np.isnan(data))))
	

	styles = ['-','--','-.','--.']
	colors = ['g','b','r','orange']
	length = len(temps)

	# print(porositiesabcavg)
	# print(yerr_abc)

	# #plot porosity vs size for all temps 
	# fig,ax = plt.subplots()
	# for i,t in enumerate(temps):
	# 	# print(porositiesabcavg)
	# 	# print(shape(porositiesabcavg))
	# 	# exit(0)
	# 	ax.errorbar(Nums,porositiesabcavg[:,i],yerr=yerr_abc[:,i],\
	# 				label="t={}K".format(t),zorder=5)
	# 				# label="t={}K".format(t),color=colors[0],linestyle=styles[i],zorder=5)

	# ax.set_xlabel('Number of particles')
	# # ax.set_title('Measure of porosity vs agg size')
	# ax.set_ylabel('Rabc Porosity')
	# # ax.set_legend(['Rabc','RKBM'])
	# # plt.errorbar(temps,)
	# ax.set_xscale('log')

	# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# # ax2.invert_yaxis()

	# fig.legend()
	# plt.savefig("figures/abcVsNum.png")
	# if show_plots:
	# 	plt.show()
	# plt.close("all")
	# # exit(0)

	# # #plot porosity vs size for all temps 
	# fig,ax = plt.subplots()
	# for i,t in enumerate(temps):
	# 	ax.errorbar(Nums,porositiesKBMavg[:,i],yerr=yerr_KBM[:,i],\
	# 				label="t={}K".format(t),zorder=5)
	# 				# label="t={}K".format(t),color=colors[0],linestyle=styles[i],zorder=5)

	# ax.set_xlabel('Number of particles')
	# # ax.set_title('Measure of porosity vs agg size')
	# ax.set_ylabel('RKBM Porosity')
	# # ax.set_legend(['Rabc','RKBM'])
	# # plt.errorbar(temps,)
	# ax.set_xscale('log')

	# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# # ax2.invert_yaxis()

	# fig.legend()
	# plt.savefig("figures/KBMVsNum.png")
	# if show_plots:
	# 	plt.show()
	# plt.close("all")

	# # #plot FD vs size for all temps 
	# fig,ax = plt.subplots()
	# for i,t in enumerate(temps):
	# 	ax.errorbar(Nums,FD_dataavg[:,i],yerr=yerr_FD[:,i],\
	# 				label="t={}K".format(t),zorder=5)
	# 				# label="t={}K".format(t),color=colors[0],linestyle=styles[i],zorder=5)

	# ax.set_xlabel('Number of particles')
	# # ax.set_title('Measure of Fractal Dimension vs agg size')
	# ax.set_ylabel('FD')
	# # ax.set_legend(['Rabc','RKBM'])
	# # plt.errorbar(temps,)
	# ax.set_xscale('log')

	# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# # ax2.invert_yaxis()

	# fig.legend()
	# plt.savefig("figures/FDVsNum.png")
	# if show_plots:
	# 	plt.show()
	# plt.close("all")

	# # print(contactsavg[:,0])
	# # print(yerr_ca[:,0])

	# # #plot num contacts vs size for all temps 
	# fig,ax = plt.subplots()
	# for i,t in enumerate(temps):
	# 	ax.errorbar(Nums,contactsavg[:,i],yerr=yerr_ca[:,i],\
	# 				label="t={}K".format(t),zorder=5)
	# 				# label="t={}K".format(t),color=colors[0],linestyle=styles[i],zorder=5)

	# ax.set_xlabel('Number of particles')
	# # ax.set_title('Averge contacts vs agg size')
	# ax.set_ylabel('Number of contacts')
	# # ax.set_legend(['Rabc','RKBM'])
	# # plt.errorbar(temps,)
	# ax.set_xscale('log')

	# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# # ax2.invert_yaxis()

	# fig.legend()
	# plt.savefig("figures/ContactsVsNum.png")
	# if show_plots:
	# 	plt.show()
	# plt.close("all")

	# # #plot each size separately
	# for i,N in enumerate(Nums):
	# 	if N == 300:
	# 		a = attempts300
	# 	else:
	# 		a = attempts
	# 	fig,ax = plt.subplots()
	# 	# print(porositiesabc[i])
	# 	ax2 = ax.twinx()
	# 	ax.errorbar(temps,data[i*length+1],yerr=data[i*length+2],\
	# 				label="Rabc",color=colors[0],linestyle=styles[i],zorder=5)
	# 	ax.errorbar(temps,data[i*length+3],yerr=data[i*length+4],\
	# 				label="RKBM",color=colors[1],linestyle=styles[i],zorder=5)
	# 	ax2.errorbar(temps,data[i*length+5],yerr=data[i*length+6],\
	# 				label="FD",color=colors[2],linestyle=styles[i],zorder=5)
		
	# 	ax.set_xlabel('Temperature in K')
	# 	# ax.set_title('Porosity average over {} sims N={}'.format(len(a),N))
	# 	ax.set_ylabel('Porosity')
	# 	# ax.set_legend(['Rabc','RKBM'])
	# 	# plt.errorbar(temps,)
	# 	ax.set_xscale('log')

	# 	# ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# 	# ax2.invert_yaxis()
	# 	ax2.set_ylabel('Avg Fractal Dimension')

	# 	fig.legend()
	# 	plt.savefig("figures/FractDimandPorosity_N{}.png".format(N))
	# 	if show_plots:
	# 		plt.show()
	#	plt.close("all")

	# plot each porosity measure separately
	plt.rcParams.update({'font.size': 15})
	for i,method in enumerate(["Rabc","RKBM","FD","# Contacts"]):
		# if Nums[i] == 300:
		# 	a = attempts300
		# else:
		# 	a = attempts
		
		fig,ax = plt.subplots()
		if i < 2:
			ax.set_ylabel('Porosity')
		elif i == 2:
			# ax = ax.twinx()
			ax.set_ylabel('Avg Fractal Dimension')
		elif i == 3:
			ax.set_ylabel('Avg # Contacts')

# for i,N in enumerate(Nums):
# 			headers.append('N={} abc porosity'.format(N))
# 			data[i*6,:] = porositiesabcavg[i]
# 			headers.append('N={} abc std err'.format(N)) 
# 			data[i*6+1,:] = yerr_abc[i]
# 			headers.append('N={} KBM porosity'.format(N))
# 			data[i*6+2,:] = porositiesKBMavg[i]
# 			headers.append('N={} KBM std err'.format(N))
# 			data[i*6+3,:] = yerr_KBM[i]
# 			headers.append('N={} Fractal Dimension'.format(N))
# 			data[i*6+4,:] = FD_dataavg[i]
# 			headers.append('N={} Fractal Dimension std err'.format(N))
# 			data[i*6+5,:] = yerr_FD[i]
# 		for i,N in enumerate(Nums):
# 			headers.append('N={} average number of contacts'.format(N))
# 			data[6*2 + i*2,:] = contactsavg[i]
# 			headers.append('N={} average number of contacts std err'.format(N))
# 			data[6*2 + 1 + i*2,:] = yerr_ca[i]

		# Plots with points
		# for j,N in enumerate(Nums):
		# 	ax.errorbar(temps,data[3*i+properties*j],yerr=data[3*i+1+properties*j],\
		# 			label="N={}".format(N),color=colors[i],\
		# 			linestyle=styles[j],marker='.',markersize=10,zorder=5)
		# 	print(f"N={N} number of completed sims:")
		# 	print(data[3*i+2+properties*j])
		# 	# ax.errorbar(temps,data[2*i+length+1],yerr=data[2*i+length+2],\
		# 	# 		label="N={}".format(Nums[1]),color=colors[i],\
		# 	# 		linestyle=styles[1],marker='.',markersize=10,zorder=5)
		# 	# ax.errorbar(temps,data[2*i+length*2+1],yerr=data[2*i+length*2+2],\
		# 	# 		label="N={}".format(Nums[2]),color=colors[i],\
		# 	# 		linestyle=styles[2],marker='.',markersize=10,zorder=5)
		# # else:
		# # 	for j,N in enumerate(Nums):
		# # 		ax.errorbar(temps,data[len(data)-len(Nums)*2+2*j],yerr=data[len(data)-len(Nums)*2+2*j+1],\
		# # 				label="N={}".format(N),color='orange',\
		# # 				linestyle=styles[j],marker='.',markersize=10,zorder=5)
		# bbox = ax.get_window_extent().transformed(fig.dpi_scale_trans.inverted())
		# # print(bbox.width, bbox.height)
		# ax.set_xlabel('Temperature in K')
		# ax.set_title('{} {} vs Temp'.format(dataset_name,method))
		# # ax.set_legend(['Rabc','RKBM'])
		# # plt.errorbar(temps,)
		# ax.set_xscale('log')

		# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
		# # ax2.invert_yaxis()
		# # if i == 1:
		# if True:
		# 	fig.legend(loc='upper right',bbox_to_anchor=(0.98, 0.97))
		# plt.tight_layout()
		# plt.savefig("{}{}_FractDimandPorosity_{}.png".format(figure_folder,dataset_name,method.replace(" ","")))
		# if show_plots:
		# 	plt.show()
		# 	print()

		#Plots with numbers of sims
		for j,N in enumerate(Nums):
			ax.errorbar(temps,data[3*i+properties*j],yerr=data[3*i+1+properties*j],\
					label="N={}".format(N),color=colors[i],\
					linestyle=styles[j],marker='.',markersize=10,zorder=5)

			for k, txt in enumerate(data[3*i+2+properties*j]):
				ax.annotate("{:0.0f}".format(txt), (temps[k], data[3*i+properties*j][k]))

		bbox = ax.get_window_extent().transformed(fig.dpi_scale_trans.inverted())
		ax.set_xlabel('Temperature in K')
		ax.set_title('{} {} vs Temp'.format(dataset_name,method))
		ax.set_xscale('log')
		if True:
			fig.legend(loc='upper right',bbox_to_anchor=(0.98, 0.97))
		plt.tight_layout()
		plt.savefig("{}{}_FractDimandPorositySimNums_{}.png".format(figure_folder,dataset_name,method.replace(" ","")))
		if show_plots:
			plt.show()

	plt.close("all")


	# # #plot each size for num contacts
	# fig,ax = plt.subplots()
	# for i,N in enumerate(Nums):
	# 	# if N == 300:
	# 	# 	a = attempts300
	# 	# else:
	# 	# 	a = attempts

	# 	ax.errorbar(temps,data[len(data)-len(Nums)*2+2*i],yerr=data[len(data)-len(Nums)*2+2*i+1],\
	# 				label="N={}".format(N),color='orange',linestyle=styles[i],zorder=5)

	# ax.set_xlabel('Temperature in K')
	# # ax.set_title('Averge number of contacts over {} sims N={}'.format(len(a),N))
	# ax.set_ylabel('# Contacts')
	# # ax.set_legend(['Rabc','RKBM'])
	# # plt.errorbar(temps,)
	# ax.set_xscale('log')

	# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# # ax2.invert_yaxis()

	# # fig.legend()
	# plt.savefig("figures/avgContacts.png")
	# if show_plots:
	# 	plt.show()
	# plt.close("all")
	# plt.rcParams.update({'font.size': 10})


	#plot all sizes together
	# fig,ax = plt.subplots()
	# ax2 = ax.twinx()

	# # length = data.shape[]
	# for i,N in enumerate(Nums):
	# 	# print('N={}\n{}'.format(N,data[i*length+1]))
	# 	# print('N2={}\n{}'.format(N,data[i*length+2]))
	# 	ax.errorbar(temps,data[i*length+1],yerr=data[i*length+2],label="Rabc,N={}".format(N),\
	# 		linestyle=styles[i],color="g",zorder=5)
	# 	ax.errorbar(temps,data[i*length+3],yerr=data[i*length+4],label="RKBM,N={}".format(N),\
	# 		linestyle=styles[i],color="b",zorder=5)
	# 	ax2.errorbar(temps,data[i*length+5],yerr=data[i*length+6],label="FD, N={}".format(N),\
	# 		linestyle=styles[i],color="r",zorder=5)
	
	# ax.set_xlabel('Temperature in K')
	# # ax.set_title('Average Porosity')
	# ax.set_ylabel('Porosity')
	# # ax.set_legend(['Rabc','RKBM'])
	# # plt.errorbar(temps,)
	# ax.set_xscale('log')

	# # ax2 = ax.twinx()
	# # ax2.errorbar(temps,FD_dataavg,yerr=yerr2,label="Frac Dim",color='r',zorder=0)
	# # ax2.invert_yaxis()
	# ax2.set_ylabel('Avg Fractal Dimension')

	# fig.legend()
	# plt.savefig("figures/FractDimandPorosity_all.png")
	# if show_plots:
	# 	plt.show()





	# fig, ax = plt.
