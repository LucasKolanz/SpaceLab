import os
import fnmatch
import os
import glob
import json

relative_path = ""
relative_path = '/'.join(__file__.split('/')[:-1]) + '/' + relative_path
project_path = os.path.abspath(relative_path) + '/'
def delete_files(file_paths):
    """
    Attempts to delete each file in the given list of file paths.
    
    :param file_paths: List of paths to the files to be deleted.
    """
    for path in file_paths:
        try:
            os.remove(path)
            print(f"File deleted: {path}")
        except FileNotFoundError:
            print(f"File not found, skipping: {path}")
        except PermissionError:
            print(f"Permission denied, skipping: {path}")
        except Exception as e:
            print(f"Error deleting file {path}: {e}")

def get_all_files(root_dir,pattern):
    all_of_them = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if fnmatch.fnmatch(file, pattern):  # Match using glob pattern
                all_of_them.append(root+'/'+file)

    return all_of_them


def get_all_specific_files(root_dir, pattern, jobset_name, jobset_range=30):
    allfiles = []

    if type(jobset_range) is int:
        jobset_range = list(range(jobset_range))
    elif type(jobset_range) is not list:
        print(f"ERROR: input variable jobset_range must be type list or int, not {type(jobset_range)}")
        exit(-1)


    dirrs = [f"{root_dir}{jobset_name}{i}/" for i in jobset_range]

    # print(dirrs)
    # exit(0)

    for dirr in dirrs:
        for dirpath, dirnames, files in os.walk(dirr):
            for file in files:
                if fnmatch.fnmatch(file, pattern):  # Match using glob pattern
                    filepath = os.path.join(dirpath, file)
                    allfiles.append(filepath)
                    # try:
                    #     os.remove(file_path)
                    #     print(f"Deleted: {file_path}")
                    # except Exception as e:
                    #     print(f"Error deleting {file_path}: {e}")
    return allfiles


def bytes_to_human_readable(num_bytes):
    """
    Converts a size in bytes to a human-readable string in KB, MB, GB, etc.
    
    :param num_bytes: Size in bytes.
    :return: Human-readable size string.
    """
    for unit in ['bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB']:
        if num_bytes < 1024.0:
            return f"{num_bytes:3.1f} {unit}"
        num_bytes /= 1024.0
    return f"{num_bytes:.1f} YB"  # Handle sizes beyond ZB

def calculate_total_space(file_paths):
    """
    Calculates the total space taken up by the files at the given paths and
    returns the size in a human-readable format.
    
    :param file_paths: List of paths to the files.
    :return: Total size of the files as a human-readable string.
    """
    total_size = 0
    for path in file_paths:
        if os.path.exists(path):
            total_size += os.path.getsize(path)
        else:
            print(f"Warning: The file at {path} does not exist and will be skipped.")
    return bytes_to_human_readable(total_size)


if __name__ == '__main__':
    # Example usage
    with open(project_path+"default_files/default_input.json",'r') as fp:
        input_json = json.load(fp)
    data_directory = input_json["data_directory"] 


    # target_directory = '/media/kolanzl/easystore/SpaceLab_data/jobsNovus/'
    # target_directory = '/media/kolanzl/easystore/SpaceLab_data/'
    target_directory = '/mnt/be2a0173-321f-4b9d-b05a-addba547276f/kolanzl/SpaceLab_data/jobsNovus/'
    # jobset_name = "const"
    # jobset_name = "const_relax"

    file_patterns = ['job_data.csv']
    # file_patterns = ['fractdim_ppb-*.csv']
    # file_patterns.append('pointcloud*.pcd')
    # file_patterns.append('Collider*.o')
    # file_patterns.append('Collider*.x')

    DELETE = False
    all_output = []
    
    for file_pattern in file_patterns:
        #how spwcific do you want to be?
        # output = get_all_specific_files(target_directory, file_pattern, jobset_name)
        output = get_all_files(target_directory, file_pattern)
        all_output.extend(output)
        space = calculate_total_space(output)
        print(f"{len(output)} files with pattern '{file_pattern}' in root directory '{target_directory}' taking up {space} ")

    # print(all_output)
    # for output in all_output:
    #     print(output.split("/")[-1])


    # if DELETE:
    #     print(f"Deleting file patterns . . .")
    #     delete_files(all_output)


    print(f"Done")