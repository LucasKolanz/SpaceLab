import sys
import h5py
import os

# file = '/home/lucas/Desktop/SpaceLab_data/jobs/h5test1/N_5/T_3/0_data.h5'

# f = h5py.File('/home/lucas/Desktop/SpaceLab_data/restartTest1/N_5/T_3/2_data.h5','r')
# f = h5py.File(file,'r')

# data = f['/energy'][:]
# print(data)
# print(len(data))


# for item in f.keys():
#     print(item)
#     for it in f[item]:
#         print(it)
#         print (item + ":", f['/'+item])
		# print (item + ":", f['/'+item][it][:])





# Replace 'file_path' with your HDF5 file path and 'dataset_name' with your dataset name
# file_path = '/home/lucas/Desktop/SpaceLab_data/jobs/restartTest1/N_5/T_3/3_data.h5'
# # file_path = '/home/lucas/Desktop/SpaceLab_data/jobs/restartTest1/N_5/T_3/2_data.h5'
# dataset_name = 'writes'

# with h5py.File(file_path, 'r') as file:
#     dataset = file[dataset_name]
#     # Assuming the value you want to read is at a specific index, for example [0,0] for a 2D dataset
#     scalar_value = dataset[()]

# print(scalar_value)


def main(argv):
	lines = -1
	data_type = ""
	data_file = argv[1]
	metaData = False

	argv = argv[2:]
	for a_i in range(len(argv)):
		if argv[a_i] == "-n":
			lines = int(argv[a_i+1])
			a_i += 1
		elif argv[a_i] == "-t":
			data_type = argv[a_i+1]
			a_i += 1
		elif argv[a_i] == "-m":
			metaData = True
		else:
			print(f"ERROR: Argument '{argv[a_i]}' not recognized.")

		if a_i == len(argv)-1:
			break



	with h5py.File(data_file,'r') as f:
		everything = f[f'/{data_type}'] 
		data = everything[:]
		print(data)
		print(len(data))

		if metaData:
			for key, value in everything.attrs.items():
				print(f"{key}: {value}")

if __name__ == '__main__':
	main(sys.argv)