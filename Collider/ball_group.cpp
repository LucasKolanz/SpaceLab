
#include <omp.h>
#include <thread>
#include <fstream>
#include <stdio.h>

#include "ball_group.hpp"
#include "../utilities/vec3.hpp"
#include "../utilities/Utils.hpp"
#include "../utilities/simple_graph.hpp"


//For testing
Ball_group::Ball_group(std::string& path,bool test)
{
    parse_input_file(path);
    attrs.relax_index = 93;
    std::string filename = find_file_name(path,attrs.relax_index);
    std::cerr<<"Filename: "<<filename<<std::endl; 
    loadSim(path, filename.substr(filename.find_last_of('/')+1,filename.size()));
    // std::cerr<<isConnected(pos,R,attrs.num_particles)<<std::endl;
}

/// @brief For creating a new ballGroup of size nBalls
/// @param nBalls Number of balls to allocate.
Ball_group::Ball_group(const int nBalls)
{
    allocate_group(nBalls);
    for (size_t i = 0; i < nBalls; i++) {
        R[i] = 1;
        m[i] = 1;
        moi[i] = calc_moi(R[i], m[i]);
    }
}

/// @brief For generating a new ballGroup of any simTypes
/// @param path is a path to the job folder
/// @param index is the index of the file to load. If index is less than 0
///        then the code will check if this is a restart and delete partial files if applicable
///        If index is greater than or equal to 0 these checks are bypassed and
///        the specified index is simply loaded (for BCCA projectile loading)
Ball_group::Ball_group(std::string& path, const int index)
{
    parse_input_file(path);


    if (attrs.typeSim == BPCA || attrs.typeSim == BCCA || attrs.typeSim == BAPA)
    {
        aggregationInit(path,index);
    }
    else if (attrs.typeSim == collider)
    {
        colliderInit(path);
        // MPIsafe_print(std::cerr,"COLLIDER NOT IMPLIMENTED. NOW EXITING . . .\n");
        // MPIsafe_exit(-1);
    }
    else if (attrs.typeSim == relax)
    {
        if (attrs.relax_index > 0)
        {
            relaxInit(path);
        }
        else
        {
            std::string message("ERROR: simType is relax but relax_index is ("+std::to_string(attrs.relax_index)+") < 0\n");
            MPIsafe_print(std::cerr,message);
        }
    }
    else
    {
        std::string message("ERROR: simType is not set.\n");
        MPIsafe_print(std::cerr,message);
    }
        
}

Ball_group::Ball_group(const Ball_group& rhs)
{


    if (attrs.num_particles != rhs.attrs.num_particles)
    {
        *this = Ball_group(rhs.attrs.num_particles);
    }

    if (this != &rhs)
    {
        attrs = rhs.attrs;
        mom = rhs.mom;
        ang_mom = rhs.ang_mom;  // Can be vec3 because they only matter for writing out to file. Can process
                                // on host.
        PE = rhs.PE;
        KE = rhs.KE;



        for (int i = 0; i < rhs.attrs.num_pairs; ++i)
        {
            distances[i] = rhs.distances[i];
        }

        for (int i = 0; i < rhs.attrs.num_particles; ++i)
        {
            pos[i] = rhs.pos[i];
            vel[i] = rhs.vel[i];
            velh[i] = rhs.velh[i];  ///< Velocity half step for integration purposes.
            acc[i] = rhs.acc[i];
            w[i] = rhs.w[i];
            wh[i] = rhs.wh[i];  ///< Angular velocity half step for integration purposes.
            aacc[i] = rhs.aacc[i];
            R[i] = rhs.R[i];      ///< Radius
            m[i] = rhs.m[i];      ///< Mass
            moi[i] = rhs.moi[i];  ///< Moment of inertia            
        }


        data = rhs.data;
    }
   

    // return *this;


}

// Initializes relax job (only new, not for restart)
void Ball_group::relaxInit(const std::string path)
{
    std::string filename = find_file_name(path,attrs.relax_index); 

    loadSim(path, filename.substr(filename.find_last_of('/')+1,filename.size()));
    zeroVel();
    // zeroAngVel();

    calc_v_collapse(); 
    if (attrs.dt < 0)
    {
        if (attrs.v_custom < 1.0)
        {
            calibrate_dt(0, 1.0);
        } //This value of 0.36 seems to work well, but a better approximation would be beneficial
        else
        {
            calibrate_dt(0, attrs.v_custom);
        }
        // calibrate_dt(0, attrs.v_custom);
    }
    simInit_cond_and_center(false);

}

void Ball_group::colliderInit(const std::string path)
{
    // parse_input_file(std::string(path));
    sim_init_two_cluster(path, attrs.projectileName, attrs.targetName);
    calc_v_collapse(); //Move this into sim_init_two_clusters so it is run when aggregates are centered at origin. Use the larger of the two if different
    calibrate_dt(0, attrs.v_custom);
    simInit_cond_and_center(true);
}

// Initializes BPCA and BCCA job for restart or new job
// index is -1 by default. If index >= 0 then this isnt a restart 
// and this shouldnt delete any files. 
void Ball_group::aggregationInit(const std::string path,const int index)
{
    int restart;
    if (index < 0)
    {
        restart = check_restart(path);
    }
    else
    {
        restart = 1;
    }

    // If the simulation is complete exit now. Otherwise, the call to 
    //find_whole_file_name will possibly delete one of the data files 
    if (restart == 2)
    {
        MPIsafe_print(std::cerr,"Simulation already complete. Now exiting. . .\n");
        MPIsafe_exit(0);
    }

    std::string filename = find_whole_file_name(path,index);

    bool just_restart = false;

    if (filename != "")
    {
        if (filename.substr(filename.size()-4,filename.size()) == ".csv")
        {
            size_t _pos = filename.find_first_of("_");
            // int file_index = stoi(filename.substr(0,filename.find_first_of("_")));
            if (filename[_pos+1] == 'R')
            {
                just_restart = true;
            }
        }
    }

    
    if (!just_restart && restart==1)
    {
        MPIsafe_print(std::cerr,std::string("Loading sim "+filename+'\n'));
        // MPIsafe_print(std::cerr,std::string("Loading sim "+path+filename+'\n'));
        loadSim(path, filename.substr(filename.find_last_of('/')+1,filename.size()));
        // loadSim(path, filename);
        calc_v_collapse(); 
        // getMass();
        if (attrs.dt < 0)
            calibrate_dt(0, attrs.v_custom);
        simInit_cond_and_center(false);
    }
    else if (restart == 0 || just_restart)
    {

        generate_ball_field(attrs.genBalls);
        // Hack - Override and creation just 2 balls position and velocity.
        if (attrs.genBalls > 0 && attrs.genBalls <= 2)
        {
            pos[0] = {0, R[0]+1.01e-6, 0};
            vel[0] = {0, 0, 0};
            if (attrs.genBalls > 1)
            {
                pos[1] = {0, -(R[1]+1.01e-6), 0};
                vel[1] = {0, 0, 0};
        
            }
        }
        else
        {
            MPIsafe_print(std::cerr,"ERROR: genBalls > 2 not yet implimented (right)?\n");
        }


        attrs.m_total = getMass();
        calc_v_collapse();

        //We can't trust v_collapse to set the correct k and dt for now. So just set it to 1 here if it is less
        if (attrs.v_custom < 1.0)
        {
            attrs.v_custom = 1.0;
        }
        
        calibrate_dt(0, attrs.v_custom);
        simInit_cond_and_center(true);
        
        
    }
    else
    {
        std::string message("ERROR: restart code '"+std::to_string(restart)+"' not recognized.\n");
        MPIsafe_print(std::cerr,message);

        MPIsafe_exit(-1);
    }
}



Ball_group& Ball_group::operator=(const Ball_group& rhs)
{

    attrs = rhs.attrs;

    mom = rhs.mom;
    ang_mom = rhs.ang_mom;  // Can be vec3 because they only matter for writing out to file. Can process
                            // on host.

    PE = rhs.PE;
    KE = rhs.KE;

    distances = rhs.distances;

    pos = rhs.pos;
    vel = rhs.vel;
    velh = rhs.velh;  ///< Velocity half step for integration purposes.
    acc = rhs.acc;
    w = rhs.w;
    wh = rhs.wh;  ///< Angular velocity half step for integration purposes.
    aacc = rhs.aacc;
    R = rhs.R;      ///< Radius
    m = rhs.m;      ///< Mass
    moi = rhs.moi;  ///< Moment of inertia


    data = rhs.data;

    return *this;
    
}



void Ball_group::init_data(int counter = 0)
{

    if (data != nullptr)
    {
        delete data;
        data = nullptr; 
    }

    std::string type = "";
    
    if (attrs.typeSim == relax)
    {
        type = "RELAX";
    }

    std::string sav_file;
    if (attrs.data_type == 0) //h5
    {
        sav_file = attrs.output_folder+std::to_string(counter)+"_"+type+"data."+attrs.filetype;
    }
    else if (attrs.data_type == 1) //csv
    {
        sav_file = attrs.output_folder+std::to_string(counter)+"_"+type+".csv";
    }
    else
    {
        std::string message("ERROR: data_type '"+attrs.filetype+"' not supported.\n"); 
        MPIsafe_print(std::cerr,message);
        MPIsafe_exit(EXIT_FAILURE);
    }
    data = new DECCOData(sav_file,\
                        attrs.num_particles,attrs.steps/attrs.skip+1,attrs.steps);
    
}


std::string Ball_group::data_type_from_input(const std::string location)
{
    json inputs = getJsonFromFolder(location);
    if (inputs.contains("dataFormat"))
    {
        return inputs["dataFormat"];
    }
    else
    {
        std::cerr<<"WARNING: dataFormat input does not exist."<<std::endl;
        return "WARNING: dataFormat input does not exist.";
    }
}

void Ball_group::set_seed_from_input(const std::string location)
{
    json inputs = getJsonFromFolder(location);
    if (inputs.contains("seed"))
    {
        if (inputs["seed"] == std::string("default"))
        {
            attrs.seed = static_cast<unsigned int>(time(nullptr));
        }
        else
        {
            attrs.seed = static_cast<unsigned int>(inputs["seed"]);
        }
    }
    else
    {
        MPIsafe_print(std::cerr,std::string("ERROR: no 'seed' in input file. Now exiting . . .\n"));
        MPIsafe_exit(-1);
    }

    if (getRank() == 0)
    {
        std::ofstream seedFile;
        seedFile.open(attrs.output_folder+"seedFile.txt",std::ios::app);
        seedFile<<attrs.seed<<std::endl;
        seedFile.close();
    }
    
    
    MPIsafe_print(std::cerr,std::string("Writing seed '"+std::to_string(attrs.seed)+"' to seedFile.txt\n"));
    
    seed_generators(attrs.seed);
}

//Parses input.json file that is in the same folder the executable is in
void Ball_group::parse_input_file(std::string location)
{

    MPIsafe_print(std::cerr,"STARTING PARSE_INPUT FILE");

    // if (location == "")
    // {
    //     try {
    //         fs::path currentPath = fs::current_path();
    //         location = currentPath.string() + "/";
    //     } catch (const fs::filesystem_error& e) {
    //         MPIsafe_print(std::cerr,std::string("Error getting current directory: " + std::string(e.what()) + '\n'));
    //         exit(-1);
    //     }
    // }
    // // std::string s_location(location);
    // std::string json_file = location + "input.json";
    // std::ifstream ifs(json_file);
    // json inputs = json::parse(ifs);
    json inputs = getJsonFromFolder(location);
    MPIsafe_print(std::cerr,std::string("Parsing input file: "+location+"input.json\n"));

    set_attribute(inputs,"output_folder",attrs.output_folder);
    // attrs.output_folder = inputs["output_folder"];
    if (attrs.output_folder == "")
    {
        MPIsafe_print(std::cerr,"ERROR: output_folder not specified in input.json. This cannot be left blank.");
        MPIsafe_exit(-1);
    }

    set_attribute(inputs,"data_directory",attrs.data_directory);
    // attrs.data_directory = inputs["data_directory"];
    set_attribute(inputs,"random_folder_template",attrs.random_folder_template);
    // if (inputs.contains("random_folder_template"))
    // {
    //     attrs.random_folder_template = inputs["random_folder_template"];
    // }

    std::string temp_sim_type = "";
    set_attribute(inputs,"simType",temp_sim_type);
    std::string temp_symmetric = "";
    set_attribute(inputs,"symmetric",temp_symmetric);
    if (temp_sim_type == "BPCA")
    {
        attrs.typeSim = BPCA;
    }
    else if (temp_sim_type == "BCCA")
    {
        attrs.typeSim = BCCA;
        if (temp_symmetric == "True" || temp_symmetric == "true" || temp_symmetric == "1")
        {
            attrs.symmetric = true;
        }
        else if (temp_symmetric == "False" || temp_symmetric == "false" || temp_symmetric == "0")
        {
            attrs.symmetric = false;
        }
        else
        {
            MPIsafe_print(std::cerr,"Simulation input 'symmetric' must be either true or false. Setting to 'true' as default.");
            attrs.symmetric = true;
        }
    }
    else if (temp_sim_type == "BAPA")
    {
        attrs.typeSim = BAPA;
    }
    else if (temp_sim_type == "collider")
    {
        attrs.typeSim = collider;
    }
    else if (temp_sim_type == "relax")
    {
        attrs.typeSim = relax;
        set_attribute(inputs,"relaxIndex",attrs.relax_index);
        // attrs.relax_index = inputs["relaxIndex"];
    }

    std::string temp_dataFormat = "";
    set_attribute(inputs,"dataFormat",temp_dataFormat);
    if (temp_dataFormat == "h5" || temp_dataFormat == "hdf5")
    {
        attrs.data_type = 0;
        attrs.filetype = "h5";
    }
    else if (temp_dataFormat == "csv")
    {
        attrs.data_type = 1;
        attrs.filetype = "csv";
    }


    

    std::string temporary_distribution = "";
    set_attribute(inputs,"radiiDistribution",temporary_distribution);
    // std::string temporary_distribution = inputs["radiiDistribution"];
    std::transform(temporary_distribution.begin(), temporary_distribution.end(), temporary_distribution.begin(), ::tolower);
    if (temporary_distribution == "lognormal" || temporary_distribution == "lognorm")
    {
        attrs.radiiDistribution = logNorm;
    }
    else if (temporary_distribution == "const" || temporary_distribution == "constant")
    {
        attrs.radiiDistribution = constant;
    }
    else
    {
        std::cerr<<"WARNING: radiiDistribution not specified. Defaulting to 'constant'"<<std::endl;
        attrs.radiiDistribution = constant;
    }

    set_attribute(inputs,"N",attrs.N);
    // attrs.N = inputs["N"];
    set_attribute(inputs,"M",attrs.M);
    // if (inputs.contains("M"))
    // {
    //     attrs.M = inputs["M"];
    // }
    set_attribute(inputs,"dynamicTime",attrs.dynamicTime);
    // attrs.dynamicTime = inputs["dynamicTime"];
    set_attribute(inputs,"G",attrs.G);
    // attrs.G = inputs["G"];
    set_attribute(inputs,"density",attrs.density);
    // attrs.density = inputs["density"];
    set_attribute(inputs,"u_s",attrs.u_s);
    // attrs.u_s = inputs["u_s"];
    set_attribute(inputs,"u_r",attrs.u_r);
    // attrs.u_r = inputs["u_r"];
    set_attribute(inputs,"sigma",attrs.sigma);
    // attrs.sigma = inputs["sigma"];
    set_attribute(inputs,"Y",attrs.Y);
    // attrs.Y = inputs["Y"];
    set_attribute(inputs,"cor",attrs.cor);
    // attrs.cor = inputs["cor"];
    set_attribute(inputs,"simTimeSeconds",attrs.simTimeSeconds);
    // attrs.simTimeSeconds = inputs["simTimeSeconds"];
    set_attribute(inputs,"timeResolution",attrs.timeResolution);
    // attrs.timeResolution = inputs["timeResolution"];
    attrs.fourThirdsPiRho = 4. / 3. * pi * attrs.density;
    set_attribute(inputs,"scaleBalls",attrs.scaleBalls);
    // attrs.scaleBalls = inputs["scaleBalls"];
    set_attribute(inputs,"maxOverlap",attrs.maxOverlap);
    // attrs.maxOverlap = inputs["maxOverlap"];
    set_attribute(inputs,"KEfactor",attrs.KEfactor);
    // attrs.KEfactor = inputs["KEfactor"];

    set_attribute(inputs,"MPInodes",attrs.MAXMPInodes);
    // attrs.MAXMPInodes = inputs["MPInodes"];
    attrs.MPInodes = attrs.MAXMPInodes;
    set_attribute(inputs,"OMPthreads",attrs.MAXOMPthreads);
    attrs.MAXOMPthreads = inputs["OMPthreads"];
    attrs.OMPthreads = attrs.MAXOMPthreads;
 
    //Specifying one of these will modify the velocity of the collision
    //If both are specified, then print out a warning and use eta.
    //However, v_custom can't be set until you know the mass of the projectile
    //if you are setting based on temp, so we do this in the init functions.
    set_attribute(inputs,"temp",attrs.temp);
    // attrs.temp = inputs["temp"]; 
    set_attribute(inputs,"eta",attrs.eta);
    // if (inputs.contains("eta"))
    // {
    //     attrs.eta = inputs["eta"]; 
    // }

    std::string temp_v_custom = "";    
    set_attribute(inputs,"v_custom",temp_v_custom);
    if (temp_v_custom == std::string("default") || temp_v_custom == "")
    {
        attrs.v_custom = -1;
    }
    else
    {
        attrs.v_custom = std::stod(temp_v_custom);
    }

    double temp_kConst = -1.0;
    set_attribute(inputs,"kConsts",temp_kConst);
    attrs.kConsts = temp_kConst * (attrs.fourThirdsPiRho / (attrs.maxOverlap * attrs.maxOverlap));
    set_attribute(inputs,"impactParameter",attrs.impactParameter);
    // attrs.impactParameter = inputs["impactParameter"];
    set_attribute(inputs,"Ha",attrs.Ha);
    // attrs.Ha = inputs["Ha"];
    double temp_h_min = -1.0;
    set_attribute(inputs,"h_min",temp_h_min);
    attrs.h_min = temp_h_min * attrs.scaleBalls;

    std::string temp_cone = "default";
    set_attribute(inputs,"cone",temp_cone);
    if (temp_cone == std::string("default"))
    {
        attrs.cone = pi/2;
    }
    else
    {
        attrs.cone = std::stod(temp_cone);
    }

    set_attribute(inputs,"properties",attrs.properties);
    // attrs.properties = inputs["properties"];
    set_attribute(inputs,"genBalls",attrs.genBalls);
    // attrs.genBalls = inputs["genBalls"];
    set_attribute(inputs,"attempts",attrs.attempts);
    // attrs.attempts = inputs["attempts"];
    set_attribute(inputs,"skip",attrs.skip);
    // attrs.skip = inputs["skip"];
    set_attribute(inputs,"steps",attrs.steps);
    // attrs.steps = inputs["steps"];
    set_attribute(inputs,"dt",attrs.dt);
    // attrs.dt = inputs["dt"];
    set_attribute(inputs,"kin",attrs.kin);
    // attrs.kin = inputs["kin"];
    set_attribute(inputs,"kout",attrs.kout);
    // attrs.kout = inputs["kout"];

    std::string temp_spaceRange = "default";
    set_attribute(inputs,"spaceRange",temp_spaceRange);
    if (temp_spaceRange == std::string("default"))
    {
        attrs.spaceRange = 4 * std::pow(
                        (1. / .74 * attrs.scaleBalls * attrs.scaleBalls * attrs.scaleBalls * attrs.genBalls),
                        1. / 3.); 
    }
    else
    {
        attrs.spaceRange = std::stod(temp_spaceRange);
    }

    std::string temp_spaceRangeIncrement = "default";
    set_attribute(inputs,"spaceRangeIncrement",temp_spaceRangeIncrement);
    if (temp_spaceRangeIncrement == std::string("default"))
    {
        attrs.spaceRangeIncrement = attrs.scaleBalls * 3;
    }
    else
    {
        attrs.spaceRangeIncrement = std::stod(temp_spaceRangeIncrement);
    }

    set_attribute(inputs,"z0Rot",attrs.z0Rot);
    // attrs.z0Rot = inputs["z0Rot"];
    set_attribute(inputs,"y0Rot",attrs.y0Rot);
    // attrs.y0Rot = inputs["y0Rot"];
    set_attribute(inputs,"z1Rot",attrs.z1Rot);
    // attrs.z1Rot = inputs["z1Rot"];
    set_attribute(inputs,"y1Rot",attrs.y1Rot);
    // attrs.y1Rot = inputs["y1Rot"];

    set_attribute(inputs,"simTimeElapsed",attrs.simTimeElapsed);
    // attrs.simTimeElapsed = inputs["simTimeElapsed"];
    set_attribute(inputs,"projectileName",attrs.projectileName);
    // attrs.projectileName = inputs["projectileName"];
    set_attribute(inputs,"targetName",attrs.targetName);
    // attrs.targetName = inputs["targetName"];

    std::string temp_output_prefix = "default";
    set_attribute(inputs,"output_prefix",temp_output_prefix);
    // attrs.output_prefix = inputs["output_prefix"];
    if (temp_output_prefix == std::string("default"))
    {
        attrs.output_prefix = "";
    }

    set_attribute(inputs,"radiiFraction",attrs.radiiFraction);
    // attrs.radiiFraction = inputs["radiiFraction"];
    attrs.output_width = attrs.num_particles;


}

// @brief calculates the vdw force
inline double Ball_group::calc_VDW_force_mag(const double Ra,const double Rb,const double h)
{
    const double h2 = h * h;
    // constexpr double h2 = h * h;
    const double twoRah = 2 * Ra * h;
    const double twoRbh = 2 * Rb * h;
    return attrs.Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
             ((h + Ra + Rb) / ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
                               (h2 + twoRah + twoRbh + 4 * Ra * Rb) *
                               (h2 + twoRah + twoRbh + 4 * Ra * Rb)));

}

// // @breif calculates the mu scaling factor for all pairs of particle sizes
// void Ball_group::calc_mu_scale_factor()
// {
//     int e;
//     for (int A = 1; A < attrs.num_particles; ++A)
//     {
//         for (int B = 0; B < A; ++B)
//         {   
//             e = static_cast<unsigned>(A * (A - 1) * .5) + B;  // a^2-a is always even, so this works.
//             u_scale[e] = calc_VDW_force_mag(R[A],R[B],h_min_physical)/
//                                 calc_VDW_force_mag(R[A],R[B],h_min);  
//         }
//     }
// }

////////////////////////////////////
// void Ball_group::zeroSaveVals()
// {
//     int size = attrs.num_particles*attrs.num_particles;
//     for (int i = 0; i < size; ++i)
//     {
//         vdwForce[i] = {0,0,0};
//         // elasticForce[i] = {0,0,0};
//         // slideForce[i] = {0,0,0};
//         // rollForce[i] = {0,0,0};
//         // torqueForce[i] = {0,0,0};
//     }
//     // for (int i = 0; i < num_particles; ++i)
//     // {
//     //     // if (i < num_particles)
//     //     // {
//     //     // distB3[i] = 0.0;
//     //     // }
//     //     // slidDir[i] = {0,0,0};
//     //     // rollDir[i] = {0,0,0};
//     //     // inout[i] = 0.0;
//     //     // slidB3[i] = {0,0,0};
//     //     // rollB3[i] = {0,0,0};
//     //     // // slidFric[i] = {0,0,0};
//     //     // rollFric[i] = {0,0,0};
//     // }
// }
////////////////////////////////////

void Ball_group::calibrate_dt(int const Step, const double& customSpeed = -1.)
{
    const double dtOld = attrs.dt;


    std::string message = "";
    if (customSpeed > 0.) {
        updateDTK(customSpeed);
        message += "CUSTOM SPEED: " + std::to_string(customSpeed) + '\n';
    } else {
        // std::cerr << vCollapse << " <- vCollapse | Lazz Calc -> " << M_PI * M_PI * G * pow(density, 4.
        // / 3.) * pow(mTotal, 2. / 3.) * rMax;

        attrs.v_max = getVelMax();

        message += '\n';

        // Take whichever velocity is greatest:
        message += dToSci(attrs.v_collapse) + " = vCollapse | vMax = " + dToSci(attrs.v_max);
        if (attrs.v_max < attrs.v_collapse) { attrs.v_max = attrs.v_collapse; }

        // if (attrs.v_max < 1.0) { attrs.v_max = 1.0; }

        if (attrs.v_max < attrs.v_max_prev) {
            updateDTK(attrs.v_max);
            attrs.v_max_prev = attrs.v_max;
            message += "\nk: " + dToSci(attrs.kin) + "\tdt: " + dToSci(attrs.dt) + '\n';
        }
    }
    MPIsafe_print(std::cerr,message);

    message = "";

    if (Step == 0 or dtOld < 0) {
        attrs.steps = static_cast<unsigned long long>(attrs.simTimeSeconds / attrs.dt) + 1;
        if (attrs.steps < 0)
        {
            message += "ERROR: STEPS IS NEGATIVE.\n";
            message += "simTimeSeconds/dt = " + std::to_string(attrs.simTimeSeconds / attrs.dt)+'\n';
            message += "casted simTimeSeconds/dt (steps) = " + std::to_string(static_cast<int>(attrs.simTimeSeconds / attrs.dt))+'\n';
            message += "Exiting program now.\n";
            MPIsafe_print(std::cerr,message);
            exit(-1);
        }

        message += "\tInitial Steps: " + std::to_string(attrs.steps) + '\n';
    } else {
        attrs.steps = static_cast<unsigned long long>(dtOld / attrs.dt) * (attrs.steps - Step) + Step;
        if (attrs.steps < 0)
        {
            message += "ERROR: STEPS IS NEGATIVE.\n";
            message += "dtOld/dt = " + std::to_string(dtOld / attrs.dt) + '\n';
            message += "(steps - Step) = " + std::to_string(attrs.steps - Step) + '\n';
            message += "Step = " + std::to_string(Step) + '\n';
            message += "Final steps = " + std::to_string(static_cast<unsigned long long>(dtOld / attrs.dt) * (attrs.steps - Step) + Step) + '\n';
            message += "Exiting program now.'\n'";
            MPIsafe_print(std::cerr,message);
            exit(-1);
        }
        message += "\tSteps: " + std::to_string(attrs.steps);
    }
    MPIsafe_print(std::cerr,message);

    message = "";

    if (attrs.timeResolution / attrs.dt > 1.) {
        attrs.skip = static_cast<int>(floor(attrs.timeResolution / attrs.dt));
        message += "\tSkip: " + std::to_string(attrs.skip) + '\n';
    } else {
        message += "Desired time resolution is lower than dt. Setting to 1 second per skip.\n";
        attrs.skip = static_cast<int>(floor(1. / attrs.dt));
    }
    MPIsafe_print(std::cerr,message);
}

// todo - make bigger balls favor the middle, or, smaller balls favor the outside.
/// @brief Push balls apart until no overlaps
void Ball_group::pushApart() const
{
    MPIsafe_print(std::cerr,std::string("Separating spheres - Current max overlap:\n"));
    /// Using acc array as storage for accumulated position change.
    int* counter = new int[attrs.num_particles];
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        acc[Ball] = {0, 0, 0};
        counter[Ball] = 0;
    }

    double overlapMax = -1;
    const double pseudoDT = attrs.r_min * .1;
    int step = 0;

    while (true) {
        // if (step % 10 == 0)
        //{
        //  simDataWrite("pushApart_");
        //}

        for (int A = 0; A < attrs.num_particles; A++) {
            for (int B = A + 1; B < attrs.num_particles; B++) {
                // Check for Ball overlap.
                vec3 rVecab = pos[B] - pos[A];
                vec3 rVecba = -1 * rVecab;
                const double dist = (rVecab).norm();
                const double sumRaRb = R[A] + R[B];
                const double overlap = sumRaRb - dist;

                if (overlapMax < overlap) { overlapMax = overlap; }

                if (overlap > 0) {
                    acc[A] += overlap * (rVecba / dist);
                    acc[B] += overlap * (rVecab / dist);
                    counter[A] += 1;
                    counter[B] += 1;
                }
            }
        }

        for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
            if (counter[Ball] > 0) {
                pos[Ball] += acc[Ball].normalized() * pseudoDT;
                acc[Ball] = {0, 0, 0};
                counter[Ball] = 0;
            }
        }

        if (overlapMax > 0) {
            MPIsafe_print(std::cerr,std::string(std::to_string(overlapMax) + "                        \r"));//Why is there a \r here? Keeping until I know
        } else {
            MPIsafe_print(std::cerr,"\nSuccess!\n");
            break;
        }
        overlapMax = -1;
        step++;
    }
    delete[] counter;
}


//This function could use some work
//I believe it is supposed to calculate an approximate v_collapse so that
//dt can be set based on what velocity is the fastest (v_collapse, v_max, or v_custom if it is specified)
//At the moment, it seems to get stuck in the while loops for certain circumstances.
//I modified this function from Job's version to use vdw force instead of just gravity.
void Ball_group::calc_v_collapse()
{
    // Sim fall velocity onto cluster:
    // vCollapse shrinks if a ball escapes but velMax should take over at that point, unless it is
    // ignoring far balls.

    double twoRminh;
    double twoRmaxh;
    double vdwforce;
    double distance,distance2;
    double m_min = getMmin();
    double position = 0;
    double temp_dt = 0.1;
    int max_count = 10000;
    int count = 0;

    //The do while loop takes care of the case when 0.1 is way too big for dt 
    //(particularly with vdw forces) and the inner loop overshoots too much 
    do
    {
        position = 0;
        attrs.v_collapse = 0;

        // while (position < attrs.initial_radius) {
        while (position < attrs.initial_radius-attrs.h_min && count < max_count) {
            // todo - include vdw!!!

            //This is like the smallest ball falling into the largest ball due only to vdw force
            //starting at initial_radius to h_min
            distance = attrs.initial_radius-attrs.h_min-position;
            distance2 = distance*distance;
            twoRminh = 2 * attrs.r_min * distance;
            twoRmaxh = 2 * attrs.r_max * distance;
            vdwforce = attrs.Ha / 6 * 64 * attrs.r_max * attrs.r_max * attrs.r_max * attrs.r_min * attrs.r_min * attrs.r_min *
                                     ((attrs.h_min + attrs.r_max + attrs.r_min) / ((distance2 + twoRmaxh + twoRminh) * (distance2 + twoRmaxh + twoRminh) *
                                                                 ((distance2 + twoRmaxh + twoRminh) + 4 * attrs.r_max * attrs.r_min) *
                                                                 ((distance2 + twoRmaxh + twoRminh) + 4 * attrs.r_max * attrs.r_min)));
            attrs.v_collapse += vdwforce/m_min * temp_dt;


            //v_collapse due to gravity
            //Why is the radius always initial_radius and not initial_radius - position?
            // attrs.v_collapse += attrs.G * attrs.m_total / (attrs.initial_radius * attrs.initial_radius) * temp_dt;
            
            position += attrs.v_collapse * temp_dt;
            count++;
        }
        temp_dt /= 2;
    }while (position > (attrs.initial_radius-attrs.h_min)*2 && count < max_count);

    if (count >= max_count)
    {
        attrs.v_collapse = 1.0;
        MPIsafe_print(std::cerr,"WARNING: maximum iterations of calculating v_collapse hit, setting v_collapse to 1.0 cm/s.\n");
    }
    else
    {
        attrs.v_collapse = fabs(attrs.v_collapse);
    }

    // std::cerr<<"finish calc collapse: "<<attrs.v_collapse<<std::endl;
}

/// get max velocity
[[nodiscard]] double Ball_group::getVelMax()
{
    attrs.v_max = 0;

    // todo - make this a manual set true or false to use soc so we know if it is being used or not.
    if (attrs.soc > 0) {
        int counter = 0;
        for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
            if (vel[Ball].norm() > attrs.v_max) 
            { 
                attrs.v_max = vel[Ball].norm();
            }
            /////////////////SECTION COMMENTED FOR ACCURACY TESTS
            // Only consider balls moving toward com and within 4x initial radius around it.
            // const vec3 fromCOM = pos[Ball] - getCOM();
            // if (acos(vel[Ball].normalized().dot(fromCOM.normalized())) > cone && fromCOM.norm() < soc) {
            //     if (vel[Ball].norm() > v_max) { v_max = vel[Ball].norm(); }
            // } else {
            //     counter++;
            // }
        }

        // MPIsafe_print(std::cerr,'(' + std::to_string(counter) + " spheres ignored"+ ") \n");
    } else {
        for (int Ball = 0; Ball < attrs.num_particles; Ball++) {

            if (vel[Ball].norm() > attrs.v_max) 
            { 
                attrs.v_max = vel[Ball].norm();
            }
        }

        // Is vMax for some reason unreasonably small? Don't proceed. Probably a finished sim.
        // This shouldn't apply to extremely destructive collisions because it is possible that no
        // particles are considered, so it will keep pausing.
        if (attrs.v_max < 1e-10) {
            MPIsafe_print(std::cerr,"\nMax velocity in system is less than 1e-10.\n");
            system("pause");
        }
    }

    return attrs.v_max;
}

double Ball_group::get_soc()
{
    return attrs.soc;
}

void Ball_group::calc_helpfuls(const bool includeRadius)
{
    attrs.r_min = getRmin();
    attrs.r_max = getRmax();
    attrs.m_total = getMass();

    if (includeRadius) {attrs.initial_radius = getRadius(getCOM());}
    attrs.soc = 4 * attrs.r_max + attrs.initial_radius;
    // soc = -1;
}   

// Kick ballGroup (give the whole thing a velocity)
void Ball_group::kick(const vec3& vec) const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { vel[Ball] += vec; }
}

// move ballGroup (move current position by vec)
void Ball_group::move(const vec3& vec) const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { pos[Ball] += vec; }
}


vec3 Ball_group::calc_momentum(const std::string& of = "") const
{
    vec3 pTotal = {0, 0, 0};
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { pTotal += m[Ball] * vel[Ball]; }
    // fprintf(stderr, "%s Momentum Check: %.2e, %.2e, %.2e\n", of.c_str(), pTotal.x, pTotal.y, pTotal.z);
    return pTotal;
}

// offset cluster
void Ball_group::offset(const double& rad1, const double& rad2, const double& impactParam) const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        pos[Ball].x += (rad1 + rad2) * cos(impactParam);
        pos[Ball].y += (rad1 + rad2) * sin(impactParam);
    }
}

/// Approximate the radius of the ballGroup.
[[nodiscard]] double Ball_group::getRadius(const vec3& center) const
{
    double radius = 0;
    if (attrs.num_particles > 1) {
        for (size_t i = 0; i < attrs.num_particles; i++) {
            const auto this_radius = (pos[i] - center).norm();
            if (this_radius > radius) radius = this_radius;
        }
    } else {
        radius = R[0];
    }

    return radius;
}

// Update Gravitational Potential Energy:
void Ball_group::updatePE()
{
    PE = 0;

    if (attrs.num_particles > 1)  // Code below only necessary for effects between balls.
    {
        for (int A = 1; A < attrs.num_particles; A++) {
            for (int B = 0; B < A; B++) {
                const double sumRaRb = R[A] + R[B];
                const double dist = (pos[A] - pos[B]).norm();
                const double overlap = sumRaRb - dist;
                double h = std::fabs(overlap);

                // Check for collision between Ball and otherBall.
                if (overlap > 0) {
                    h = attrs.h_min;
                    
                    
                    //Elastic PE
                    PE += attrs.kin * ((sumRaRb - dist) * .5) * ((sumRaRb - dist) * .5);

 
                    // Van Der Waals PE:
                    const double diffRaRb = R[A] - R[B];
                    const double z = sumRaRb + h;
                    const double two_RaRb = 2 * R[A] * R[B];
                    const double denom_sum = z * z - (sumRaRb * sumRaRb);
                    const double denom_diff = z * z - (diffRaRb * diffRaRb);
                    const double U_vdw =
                        -attrs.Ha / 6 *
                        (two_RaRb / denom_sum + two_RaRb / denom_diff + 
                        log(denom_sum / denom_diff));
                    PE += U_vdw; 

                } else {

                    const double diffRaRb = R[A] - R[B];
                    const double z = sumRaRb + h;
                    const double two_RaRb = 2 * R[A] * R[B];
                    const double denom_sum = z * z - (sumRaRb * sumRaRb);
                    const double denom_diff = z * z - (diffRaRb * diffRaRb);
                    const double U_vdw =
                        -attrs.Ha / 6 *
                        (two_RaRb / denom_sum + two_RaRb / denom_diff + log(denom_sum / denom_diff));
                    PE += U_vdw;  // Van Der Waals TURN ON FOR REAL SIM
                }
                //Gravitational PE
                // PE += -attrs.G * m[A] * m[B] / dist;
            }
        }
    } else  // For the case of just one ball:
    {
        PE = 0;
    }
}

std::string Ball_group::get_data_info()
{
    std::ostringstream out_stream;
    out_stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    
    out_stream<<"steps:"<<attrs.steps<<",skip:"<<attrs.skip;
    out_stream<<",kin:"<<attrs.kin<<",kout:"<<attrs.kout<<",dt:"<<attrs.dt;

    return out_stream.str();
}

void Ball_group::sim_init_write(int counter)
{
    MPIsafe_print(std::cerr,"Sim init write for index: " + std::to_string(counter) + '\n');
    init_data(counter);

    // if (counter > 0) { filename.insert(0, std::to_string(counter) + '_'); }

    std::vector<double> constData(data->getWidth("constants")*attrs.num_particles);
    // Write constant data:
    int pt = 0;
    int jump = data->getSingleWidth("constants");
    for (int i = 0; i < attrs.num_particles; i++) 
    {
        constData[pt] = R[i];
        constData[pt+1] = m[i];
        constData[pt+2] = moi[i];
        pt += jump;
    }

    data->Write(constData,"constants"); //THIS MUST COME BEFORE THE NEXT WRITE METADATA otherwise the h5 file wont be initiated
    if (attrs.data_type == 0) //This meta write is for restarting jobs. Only necessary for hdf5
    {
        data->WriteMeta(get_data_info(),attrs.sim_meta_data_name,"constants");
    }


    energyBuffer = std::vector<double> (data->getWidth("energy"));
    energyBuffer[0] = attrs.simTimeElapsed;
    energyBuffer[1] = PE;
    energyBuffer[2] = KE;
    energyBuffer[3] = PE+KE;
    energyBuffer[4] = mom.norm();
    energyBuffer[5] = ang_mom.norm();
    data->Write(energyBuffer,"energy");

    // Reinitialize energies for next step:
    KE = 0;
    PE = 0;
    mom = {0, 0, 0};
    ang_mom = {0, 0, 0};

    // Send position and rotation to buffer:
    ballBuffer = std::vector<double> (data->getWidth("simData"));
    pt = 0;
    jump = data->getSingleWidth("simData");
    for (int i = 0; i < attrs.num_particles; i++) 
    {
        ballBuffer[pt] = pos[i].x;
        ballBuffer[pt+1] = pos[i].y;
        ballBuffer[pt+2] = pos[i].z;
        ballBuffer[pt+3] = w[i].x;
        ballBuffer[pt+4] = w[i].y;
        ballBuffer[pt+5] = w[i].z;
        ballBuffer[pt+6] = w[i].norm();
        ballBuffer[pt+7] = vel[i].x;
        ballBuffer[pt+8] = vel[i].y;
        ballBuffer[pt+9] = vel[i].z;
        ballBuffer[pt+10] = 0;
        pt += jump;
    }
    data->Write(ballBuffer,"simData",1);

    //Initialize ballBuffer and energyBuffer to the size they should be for actual sim
    energyBuffer.clear();
    ballBuffer.clear();

    energyBuffer = std::vector<double> (data->getWidth("energy")*bufferlines);
    ballBuffer = std::vector<double> (data->getWidth("simData")*bufferlines);

    //initialize num_writes
    attrs.num_writes = 0;

    std::cerr << "\nSimulating " << attrs.steps * attrs.dt / 60 / 60 << " hours.\n";
    std::cerr << "Total mass: " << attrs.m_total << '\n';
    std::cerr << "\n===============================================================\n";

}


[[nodiscard]] vec3 Ball_group::getCOM() const
{
    if (attrs.m_total > 0) {
        vec3 comNumerator;
        for (int Ball = 0; Ball < attrs.num_particles; Ball++) { comNumerator += m[Ball] * pos[Ball]; }
        vec3 com = comNumerator / attrs.m_total;
        return com;
    } else {
        std::cerr << "Mass of cluster is zero.\n";
        exit(EXIT_FAILURE);
    }
}

[[nodiscard]] vec3 Ball_group::getVCOM() const
{
    if (attrs.m_total > 0) {
        vec3 vcomNumerator;
        for (int Ball = 0; Ball < attrs.num_particles; Ball++) { vcomNumerator += m[Ball] * vel[Ball]; }
        vec3 vcom = vcomNumerator / attrs.m_total;
        return vcom;
    } else {
        std::cerr << "Mass of cluster is zero.\n";
        exit(EXIT_FAILURE);
    }
}

void Ball_group::zeroVel() const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { vel[Ball] = {0, 0, 0}; }
}

void Ball_group::zeroAngVel() const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { w[Ball] = {0, 0, 0}; }
}

void Ball_group::to_origin() const
{
    const vec3 com = getCOM();

    move(-com);
    // for (int Ball = 0; Ball < attrs.num_particles; Ball++) { pos[Ball] -= com; }
}

// Set velocity of all balls such that the cluster spins:
void Ball_group::comSpinner(const double& spinX, const double& spinY, const double& spinZ) const
{
    const vec3 comRot = {spinX, spinY, spinZ};  // Rotation axis and magnitude
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        vel[Ball] += comRot.cross(pos[Ball] - getCOM());
        w[Ball] += comRot;
    }
}

void Ball_group::rotAll(const char axis, const double angle) const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) 
    {
        pos[Ball] = pos[Ball].rot(axis, angle);
        vel[Ball] = vel[Ball].rot(axis, angle);
        w[Ball] = w[Ball].rot(axis, angle);
    }
}

double Ball_group::calc_mass(const double& radius, const double& density)
{
    return density * 4. / 3. * 3.14159 * std::pow(radius, 3);
}

double Ball_group::calc_moi(const double& radius, const double& mass) { return .4 * mass * radius * radius; }

//Not used anywhere at the moment
Ball_group Ball_group::spawn_particles(const int count)
{
    // Load file data:
    std::cerr << "Add Particle\n";

    // Random particle to origin
    Ball_group projectile(count);
    // Particle random position at twice radius of target:
    // We want the farthest from origin since we are offsetting form origin. Not com.
    const auto cluster_radius = 3;

    const vec3 projectile_direction = rand_vec3(1).normalized();
    projectile.pos[0] = projectile_direction * (cluster_radius + attrs.scaleBalls * 4);
    projectile.w[0] = {0, 0, 0};
    // Velocity toward origin:
    projectile.vel[0] = -attrs.v_custom * projectile_direction;
    projectile.R[0] = 1e-5;  // rand_between(1,3)*1e-5;
    projectile.m[0] = attrs.density * 4. / 3. * pi * std::pow(projectile.R[0], 3);
    projectile.moi[0] = calc_moi(projectile.R[0], projectile.m[0]);

    const double3x3 local_coords = local_coordinates(to_double3(projectile_direction));
    // to_vec3(local_coords.y).print();
    // to_vec3(local_coords.z).print();
    // projectile.pos[0].print();
    for (int i = 1; i < projectile.attrs.num_particles - 3; i++) {
        const auto rand_y = rand_between(-cluster_radius, cluster_radius);
        const auto rand_z = rand_between(-cluster_radius, cluster_radius);
        // projectile.pos[i] = projectile.pos[0] + perpendicular_shift(local_coords, rand_y, rand_z);
        projectile.pos[i] = projectile.pos[0] + perpendicular_shift(local_coords, rand_y, rand_z);
        // std::cout << rand_y << '\t' << to_vec3(local_coords.y * rand_y) <<'\t'<< projectile.pos[i] <<
        // '\n';
    }
    projectile.pos[projectile.attrs.num_particles - 3] = projectile_direction * 2;
    projectile.pos[projectile.attrs.num_particles - 2] = projectile_direction * 4;
    projectile.pos[projectile.attrs.num_particles - 1] = projectile_direction * 6;

    Ball_group new_group{projectile.attrs.num_particles + attrs.num_particles};

    new_group.merge_ball_group(*this);
    new_group.merge_ball_group(projectile);

    // new_group.calibrate_dt(0, 1);
    // new_group.init_conditions();

    // new_group.to_origin();
    return new_group;
}

//@brief returns new position of particle after it is given random offset
//@param local_coords is plane perpendicular to direction of projectile
//@param projectile_pos is projectile's position before offset is applied
//@param projectile_vel is projectile's velocity
//@param projectile_rad is projectile's radius
//@returns the vector from the original particle position to the new position

//TODO:: Make this work with two aggregates instead of just a particle and aggregate
// vec3 Ball_group::random_offset(
//     const double3x3 local_coords,
//     vec3 projectile_pos,
//     vec3 projectile_vel,
//     const double projectile_rad)
// {
//     const auto cluster_radius = getRadius(vec3(0, 0, 0));
//     bool intersect = false;
//     int count = 0;
//     vec3 new_position = vec3(0,0,0);
//     do {
//         const auto rand_y = rand_between(-cluster_radius, cluster_radius);
//         const auto rand_z = rand_between(-cluster_radius, cluster_radius);
//         auto test_pos = projectile_pos + perpendicular_shift(local_coords, rand_y, rand_z);

//         count++;
//         for (size_t i = 0; i < attrs.num_particles; i++) {
//             // Check that velocity intersects one of the spheres:
//             if (line_sphere_intersect(test_pos, projectile_vel, pos[i], R[i] + projectile_rad)) {
//                 new_position = test_pos;
//                 intersect = true;
//                 break;
//             }
//         }
//     } while (!intersect);

//     return projectile_pos-new_position;
// }

//@brief gives the projectile an offset (making sure projectile and target will still collide)
//@param local_coords is plane perpendicular to direction of projectile
//@param projectile is projectile's position before offset is applied
//@param projectile_vel is projectile's velocity
//@returns the vector from the original particle position to the new position

//TODO:: Test if this works for BPCA before using it
vec3 Ball_group::random_offset(
    Ball_group &projectile,
    Ball_group &target)
{

    const auto possible_radius = target.getRadius(target.getCOM()) + projectile.getRadius(projectile.getCOM());
    const auto projectile_vcom = projectile.getVCOM();
    // const auto projectile_radius = projectile.getRadius(projectile.getCOM());
    bool intersect = false;
    int count = 0;
    vec3 shift;
    vec3 new_position = vec3(0,0,0);
    const double3x3 local_coords = local_coordinates(to_double3(projectile_vcom));
    do {
        const auto rand_y = rand_between(-possible_radius, possible_radius);
        const auto rand_z = rand_between(-possible_radius, possible_radius);
        count++;

        for (size_t i = 0; i < projectile.attrs.num_particles; ++i) {
            shift = perpendicular_shift(local_coords, rand_y, rand_z);
            auto test_pos = projectile.pos[i] + shift;
            for (size_t j = 0; j < target.attrs.num_particles; j++) {
                // Check that velocity intersects one of the spheres:
                if (line_sphere_intersect(test_pos, projectile_vcom, target.pos[j], target.R[j] + projectile.R[i])) {
                    // new_position = projectile.getCOM()+shift;
                    intersect = true;
                    break;
                }
            }
            if (intersect)
            {
                break;
            }
        }
    } while (!intersect);

    projectile.move(-shift);
    return shift;
}

//This function should calculate the Potential energy of a group.
//This Potential energy should not include elastic PE.
double Ball_group::calc_noncontact_PE()
{
    double PotE = 0;

    if (attrs.num_particles > 1)  // Code below only necessary for effects between balls.
    {
        for (int A = 1; A < attrs.num_particles; A++) {
            for (int B = 0; B < A; B++) {
                const double sumRaRb = R[A] + R[B];
                const double dist = (pos[A] - pos[B]).norm();
                const double overlap = sumRaRb - dist;
                double h = std::fabs(overlap);

                // Check for collision between Ball and otherBall.
                if (overlap > 0) {
                    h = attrs.h_min;
                } 
                // Van Der Waals PE:
                const double diffRaRb = R[A] - R[B];
                const double z = sumRaRb + h;
                const double two_RaRb = 2 * R[A] * R[B];
                const double denom_sum = z * z - (sumRaRb * sumRaRb);
                const double denom_diff = z * z - (diffRaRb * diffRaRb);
                const double U_vdw =
                    -attrs.Ha / 6 *
                    (two_RaRb / denom_sum + two_RaRb / denom_diff + 
                    log(denom_sum / denom_diff));
                PotE += U_vdw; 

                //Gravity PE:
                // PotE += -attrs.G * m[A] * m[B] / dist;
            }
        }
    } 
    return PotE;
}

//This function should calculate the Potential enery between two groups.
//This Potentail energy shouldn't include elastic PE.
double Ball_group::calc_group_noncontact_PE(Ball_group &projectile,Ball_group &target)
{
    double PotE = 0;
    const double Ra = projectile.getRadius(projectile.getCOM()) + projectile.getRmax()*2;
    const double Rb = target.getRadius(target.getCOM()) + target.getRmax()*2;
    const double sumRaRb = Ra + Rb;
    const double dist = (projectile.getCOM() - target.getCOM()).norm();
    const double overlap = sumRaRb - dist;
    const double h = projectile.getRadius(projectile.getCOM()) + projectile.getRmax()*2 + target.getRadius(target.getCOM()) + target.getRmax() * 2;

    const double diffRaRb = Ra - Rb;
    const double z = sumRaRb + h;
    const double two_RaRb = 2 * Ra * Rb;
    const double denom_sum = z * z - (sumRaRb * sumRaRb);
    const double denom_diff = z * z - (diffRaRb * diffRaRb);
    const double U_vdw =
        -attrs.Ha / 6 *
        (two_RaRb / denom_sum + two_RaRb / denom_diff + 
        log(denom_sum / denom_diff));
    PotE += U_vdw; 
    // PotE += (-G * projectile.mTotal * target.mTotal / (projectile.getCOM() - target.getCOM()).norm());
    return PotE;
}

//This function calculates v_custom to be the velocity 
//of a collision where the kinetic energy  is eta times 
//the potential energy.
double Ball_group::calc_eta_velocity(const double eta, Ball_group& projectile, Ball_group& target)
{

    const double proj_PE = projectile.calc_noncontact_PE();
    const double targ_PE = target.calc_noncontact_PE();
    const double PEsys = proj_PE + targ_PE + calc_group_noncontact_PE(projectile,target);
    const double mp = projectile.attrs.m_total;
    const double mt = target.attrs.m_total;

    const double vp = sqrt(2 * eta * fabs(PEsys) * (mp / (mt * (mp+mt)))); 

    attrs.v_custom = vp + vp*(mp/mt);

    return attrs.v_custom;

}


double Ball_group::calc_max_bolt_velocity(double temp, double mass)
{
    double a = std::sqrt(Kb*temp/mass);
    return max_bolt_dist(a); 
}

// @brief returns new ball group consisting of one particle
//        where particle is given initial conditions
//        including an random offset linearly dependant on radius 
Ball_group Ball_group::BPCA_projectile_init()
{
    // Random particle to origin
    Ball_group projectile(1);

    // Particle random position at twice radius of target:
    // We want the farthest from origin since we are offsetting form origin. Not com.
    // const auto cluster_radius = getRadius(vec3(0, 0, 0));

    // const vec3 projectile_direction = rand_unit_vec3();
    // projectile.pos[0] = projectile_direction * (cluster_radius + attrs.scaleBalls * 4);
    if (attrs.radiiDistribution == constant)
    {
        // std::cout<<"radiiFraction: "<<radiiFraction<<std::endl;
        projectile.R[0] = attrs.scaleBalls;  //MAKE BOTH VERSIONS SAME
        // projectile.R[0] = scaleBalls/radiiFraction;  //limit of 1.4// rand_between(1,3)*1e-5;
        // std::cout<<"(constant) Particle added with radius of "<<projectile.R[0]<<std::endl;
    }
    else
    {
        projectile.R[0] = lognorm_dist(attrs.scaleBalls*std::exp(-5*std::pow(attrs.lnSigma,2)/2),attrs.lnSigma);
        // std::cout<<"(lognorm) Particle added with radius of "<<projectile.R[0]<<std::endl;
    }
    projectile.w[0] = {0, 0, 0};
    projectile.m[0] = attrs.density * 4. / 3. * pi * std::pow(projectile.R[0], 3);
    projectile.moi[0] = calc_moi(projectile.R[0], projectile.m[0]);
    projectile.calc_helpfuls(true);


    // Velocity toward origin:
    //Update v_custom if temp or eta are greater than zero
    overwrite_v_custom(projectile);

    pos_and_vel_for_collision(projectile);

    std::cerr<<"projectile at end of BPCA proj init: "<<projectile.pos[0]<<std::endl;
    // projectile.vel[0] = -attrs.v_custom * projectile_direction;

    // const double3x3 local_coords = local_coordinates(to_double3(projectile_direction));
    
    // const vec3 offset = random_offset(local_coords,projectile.pos[0],projectile.vel[0],projectile.R[0]); 

    // projectile.pos[0] -= offset;


    
    return projectile;
}

//make sure projectile file with this index exists and is in a state to use.
//If it is not, wait for a while and check again, up to max wait time (in seconds). Default is 24 hours
void Ball_group::verify_projectile(const std::string projectile_folder,const int index, const double max_wait_time=86400)
{

    std::string file_base = projectile_folder+std::to_string(index);

    //check that file exists, if not wait until it exists or max wait time reached
    std::ifstream f;
    int time_slept = 0;
    int interval = 10; //seconds
    while (time_slept<=max_wait_time)
    {
        f.open(file_base+"_checkpoint.txt");
        if (f.good())
        {
            f.close();
            break;
        }
        f.close();
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        time_slept += interval;
        MPIsafe_print(std::cerr,"Waited "+std::to_string(interval)+" seconds for the projectile to checkpoint.\n");
    }

    if (time_slept>=max_wait_time)
    {
        MPIsafe_print(std::cerr,"ERROR: verify_projectile waited the max time of "+std::to_string(max_wait_time)+" seconds for projectile to checkpoint but it didn't :(. Now exiting.");
        MPIsafe_exit(-1);
    }

    MPIsafe_print(std::cerr,"Waited a total of "+std::to_string(time_slept)+" second(s) for the projectile to checkpoint.\n");
}



// @brief returns new ball group consisting of one particle
//        where particle is given initial conditions
//        including an random offset linearly dependant on radius 
Ball_group Ball_group::BCCA_projectile_init(const bool symmetric=true)
{
    // if symmetric, ball group should just be a copy
    // otherwise, get an aggregate from a differnt folder

    Ball_group projectile;


    if (symmetric)
    {
        // projectile = *this;
        projectile = Ball_group(*this);

    }
    else
    {
        // strip off the "{index}_" at the end of the file name because this constructor
        // doesnt need it
        // rand_projectile_folder = data->getFileName().substr(0,data->getFileName().length()-1-std::to_string(attrs.num_particles).length());
        std::string rand_projectile_folder = get_rand_projectile_folder(attrs.random_folder_template);
        std::cerr<<"Getting random projectile from: "<<rand_projectile_folder<<std::endl;
        
        verify_projectile(rand_projectile_folder,attrs.num_particles);

        // exit(0);
        // projectile = Ball_group(rand_projectile_file);
        projectile = Ball_group(rand_projectile_folder,attrs.num_particles);
    }

    projectile.to_origin();

    //find velocity of projectile
    // Velocity toward origin:
    //Update v_custom if temp or eta are greater than zero
    overwrite_v_custom(projectile);

    pos_and_vel_for_collision(projectile);

    // // projectile random position at twice radius of target + projectile:
    // // We want the farthest from origin since we are offsetting form origin. Not com.
    // const auto cluster_radius = getRadius(vec3(0, 0, 0)) + projectile.getRadius(vec3(0, 0, 0));


    // const vec3 projectile_direction = rand_unit_vec3();

    // for (int i = 0; i < projectile.attrs.num_particles; ++i)
    // {

    //     projectile.pos[i] += projectile_direction * (cluster_radius + attrs.scaleBalls * 4);

    //     // Velocity toward origin:
    //     projectile.vel[i] = -attrs.v_custom * projectile_direction;
        
    //     // projectile.R[0] = 1e-5;  // rand_between(1,3)*1e-5;
    //     // projectile.moi[0] = calc_moi(projectile.R[0], projectile.m[0]);
    // }

    

  
    // //TODO:: make random_offset work with two aggregates
    // //TODO:: The collision seems to happen about a random spot, not at the center of the simulation
    // // const double3x3 local_coords = local_coordinates(to_double3(projectile_direction));
    
    // // const vec3 offset = random_offset(local_coords,projectile.getCOM(),projectile.vel[0],projectile.R[0]); 

    // // for (int i = 0; i < projectile.attrs.num_particles; ++i)
    // // {
    // //     projectile.pos[i] -= offset;
    // // }

    
    return projectile;
}

// @brief returns new ball group consisting of one particle
//        where particle is given initial conditions
//        including a random offset linearly dependant on radius 
Ball_group Ball_group::BAPA_projectile_init()
{


    std::string rand_projectile_folder = get_rand_projectile_folder(attrs.random_folder_template);

    MPIsafe_print(std::cerr,"Getting projectile of index "+std::to_string(attrs.M)+" from: "+rand_projectile_folder+'\n');
    Ball_group projectile(rand_projectile_folder,attrs.M);
    // projectile.zeroVel();
    // projectile.zeroAngVel();
    
    // Ball_group projectile(attrs.M);
    // std::string filename = find_whole_file_name(rand_projectile_folder,attrs.M);
    // projectile.loadSim(rand_projectile_folder,filename.substr(filename.find_last_of('/')+1,filename.size()));
    // std::cerr<<"loaded projectile sim"<<std::flush<<std::endl;
    // projectile.calc_v_collapse(); 
    // std::cerr<<"calculated v collapse projectile"<<std::endl;
    // // getMass();
    // if (attrs.dt < 0)
    //     projectile.calibrate_dt(0, attrs.v_custom);
    // std::cerr<<"calibrate dt projectile"<<std::endl;
    // projectile.simInit_cond_and_center(false);
    // std::cerr<<"simInit_cond_and_center projectile"<<std::endl;
    // projectile.to_origin();
    // std::cerr<<"to_origin projectile"<<std::endl;
    // projectile.calc_helpfuls();
    // std::cerr<<"calc_helpfuls projectile"<<std::endl;
    
    // //find velocity of projectile
    // // Velocity toward origin:
    // //Update v_custom if temp or eta are greater than zero
    overwrite_v_custom(projectile);

    pos_and_vel_for_collision(projectile);

    // // projectile random position at twice radius of target + projectile:
    // // We want the farthest from origin since we are offsetting form origin. Not com.
    // const auto cluster_radius = getRadius(vec3(0, 0, 0)) + projectile.getRadius(vec3(0, 0, 0));


    // const vec3 projectile_direction = rand_unit_vec3();

    // for (int i = 0; i < projectile.attrs.num_particles; ++i)
    // {

    //     projectile.pos[i] += projectile_direction * (cluster_radius + attrs.scaleBalls * 4);

    //     // Velocity toward origin:
    //     projectile.vel[i] = -attrs.v_custom * projectile_direction;
        
    //     // projectile.R[0] = 1e-5;  // rand_between(1,3)*1e-5;
    //     // projectile.moi[0] = calc_moi(projectile.R[0], projectile.m[0]);
    // }

    

  

    // const double3x3 local_coords = local_coordinates(to_double3(projectile_direction));
    
    // const vec3 offset = random_offset(local_coords,projectile.getCOM(),projectile.vel[0],projectile.R[0]); 

    // for (int i = 0; i < projectile.attrs.num_particles; ++i)
    // {
    //     projectile.pos[i] -= offset;
    // }

    
    return projectile;
}

// Uses previous O as target and adds one particle to hit it:
Ball_group Ball_group::add_projectile(const simType simtype)
{
    // Load file data:
    MPIsafe_print(std::cerr,"Add Projectile\n");

    this->to_origin();

    Ball_group projectile;
    
    if (simtype == BPCA)
    {
        projectile = BPCA_projectile_init();
    }
    else if (simtype == BCCA)
    {
        projectile = BCCA_projectile_init(attrs.symmetric);
    }
    else if (simtype == BAPA)//Ballistic Aggregate as Particle Aggregation
    {
        projectile = BAPA_projectile_init();
    }
    
    // Collision velocity calculation:
    // const vec3 p_target{calc_momentum("p_target")};
    // const vec3 p_projectile{projectile.calc_momentum("p_particle")};
    // const vec3 p_total{p_target + p_projectile};
    // const double m_target{getMass()};
    // const double m_projectile{projectile.getMass()};
    // const double m_total{m_target + m_projectile};
    // const vec3 v_com = p_total / m_total;

    // // Negate total system momentum:
    // projectile.kick(-v_com);
    // kick(-v_com); 

    // std::ostringstream oss;
    // oss << "\nTarget Velocity: " << std::scientific << vel[0].norm()
    //     << "\nProjectile Velocity: " << projectile.vel[0].norm() << "\n\n";
    // MPIsafe_print(std::cerr,oss.str());

    projectile.calc_momentum("Projectile");
    calc_momentum("Target");
    // std::cerr<<"Projectile number of particles: "<<projectile.attrs.num_particles<<std::endl;
    // std::cerr<<"Target number of particles:     "<<attrs.num_particles<<std::endl;
    Ball_group new_group{projectile.attrs.num_particles + attrs.num_particles};

    int new_num_particles = projectile.attrs.num_particles + attrs.num_particles;

    new_group.merge_ball_group(*this);
    new_group.merge_ball_group(projectile);
    new_group.attrs = attrs;
    //The next line is important because the previous line overwrites the value of num_particles set in the Ball_group constructor
    new_group.attrs.num_particles = new_num_particles;

    // Hack - if v_custom is less than 1 there are problems if dt is calibrated to this
    //        if v_custom is greater than 1 you need to calibrate dt to that v_custom
    if (attrs.v_custom < 1)
    {
        new_group.calibrate_dt(0, 1);
    }
    else
    {
        new_group.calibrate_dt(0, attrs.v_custom);
    }
    

    new_group.init_conditions();
    new_group.to_origin();

    std::cerr<<"projectile pos end of add projectile: "<<new_group.pos[new_group.attrs.num_particles-1]<<std::endl;

    return new_group;
}

/// @brief Add another ballGroup into this one.
/// @param src The ballGroup to be added.
void Ball_group::merge_ball_group(const Ball_group& src,const bool includeRadius)
{
    // Copy incoming data to the end of the currently loaded data.
    std::memcpy(
        &distances[attrs.num_particles_added], src.distances, sizeof(src.distances[0]) * src.attrs.num_particles);
    std::memcpy(&pos[attrs.num_particles_added], src.pos, sizeof(src.pos[0]) * src.attrs.num_particles);
    std::memcpy(&vel[attrs.num_particles_added], src.vel, sizeof(src.vel[0]) * src.attrs.num_particles);
    std::memcpy(&velh[attrs.num_particles_added], src.velh, sizeof(src.velh[0]) * src.attrs.num_particles);
    std::memcpy(&acc[attrs.num_particles_added], src.acc, sizeof(src.acc[0]) * src.attrs.num_particles);
    std::memcpy(&w[attrs.num_particles_added], src.w, sizeof(src.w[0]) * src.attrs.num_particles);
    std::memcpy(&wh[attrs.num_particles_added], src.wh, sizeof(src.wh[0]) * src.attrs.num_particles);
    std::memcpy(&aacc[attrs.num_particles_added], src.aacc, sizeof(src.aacc[0]) * src.attrs.num_particles);
    std::memcpy(&R[attrs.num_particles_added], src.R, sizeof(src.R[0]) * src.attrs.num_particles);
    std::memcpy(&m[attrs.num_particles_added], src.m, sizeof(src.m[0]) * src.attrs.num_particles);
    std::memcpy(&moi[attrs.num_particles_added], src.moi, sizeof(src.moi[0]) * src.attrs.num_particles);
    

    // Keep track of now loaded ball set to start next set after it:
    attrs.num_particles_added += src.attrs.num_particles;

    // num_particles_added += src.num_particles;
    // radiiDistribution = src.radiiDistribution;
    // radiiFraction = src.radiiFraction;

    // //carry over folders
    // project_path = src.project_path;
    // output_folder = src.output_folder;
    // data_directory = src.data_directory;
    // projectileName = src.projectileName;
    // targetName = src.targetName;
    // output_prefix = src.output_prefix;

    // skip = src.skip;
    // steps = src.steps;

    // dt=src.dt;
    // kin=src.kin;  // Spring constant
    // kout=src.kout;
    // data = src.data;

    calc_helpfuls(includeRadius);
}

/// Allocate balls
void Ball_group::allocate_group(const int nBalls)
{
    attrs.num_particles = nBalls;

    MPIsafe_print(std::cerr,"allocating group of size: "+std::to_string(nBalls)+'\n');

    try {
        distances = new double[(attrs.num_particles * attrs.num_particles / 2) - (attrs.num_particles / 2)];

        // #ifdef MPI_ENABLE
        // #ifdef GPU_ENABLE
        //     d_accsq = new vec3[attrs.num_particles*attrs.num_particles];
        //     d_aaccsq = new vec3[attrs.num_particles*attrs.num_particles];
        // #endif
            
        pos = new vec3[attrs.num_particles];
        vel = new vec3[attrs.num_particles];
        velh = new vec3[attrs.num_particles];
        acc = new vec3[attrs.num_particles];
        w = new vec3[attrs.num_particles];
        wh = new vec3[attrs.num_particles];
        aacc = new vec3[attrs.num_particles];
        R = new double[attrs.num_particles];
        m = new double[attrs.num_particles];
        moi = new double[attrs.num_particles];

        
    } catch (const std::exception& e) {
        std::stringstream mess;
        mess << "Failed trying to allocate group. " << e.what() << '\n';
        MPIsafe_print(std::cerr,mess.str());
    }
}


/// @brief Deallocate arrays to recover memory.
void Ball_group::freeMemory() const
{
    delete[] distances;
    delete[] pos;
    delete[] vel;
    delete[] velh;
    delete[] acc;
    delete[] w;
    delete[] wh;
    delete[] aacc;
    delete[] R;
    delete[] m;
    delete[] moi;
    // #ifdef GPU_ENABLE
    //     delete[] aaccsq;
    //     delete[] accsq;
    // #endif
    // delete data;
    
}


// Initialize accelerations and energy calculations:
void Ball_group::init_conditions()
{

    // SECOND PASS - Check for collisions, apply forces and torques:
    for (int A = 1; A < attrs.num_particles; A++)  // cuda
    {
        // DONT DO ANYTHING HERE. A STARTS AT 1.
        for (int B = 0; B < A; B++) {
            const double sumRaRb = R[A] + R[B];
            const vec3 rVecab = pos[B] - pos[A];  // Vector from a to b.
            const vec3 rVecba = -rVecab;
            const double dist = (rVecab).norm();

            // Check for collision between Ball and otherBall:
            double overlap = sumRaRb - dist;

            vec3 totalForceOnA{0, 0, 0};

            // Distance array element: 1,0    2,0    2,1    3,0    3,1    3,2 ...
            int e = static_cast<int>(A * (A - 1) * .5) + B;  // a^2-a is always even, so this works.
            // double oldDist = distances[e];

            // Check for collision between Ball and otherBall.
            if (overlap > 0) {
                double k;
                k = attrs.kin;
                // Apply coefficient of restitution to balls leaving collision.
                // if (dist >= oldDist) {
                //     k = kout;
                // } else {
                //     k = kin;
                // }

                // Cohesion (in contact) h must always be h_min:
                // constexpr double h = h_min;
                const double h = attrs.h_min;
                const double Ra = R[A];
                const double Rb = R[B];
                const double h2 = h * h;
                // constexpr double h2 = h * h;
                const double twoRah = 2 * Ra * h;
                const double twoRbh = 2 * Rb * h;
                const vec3 vdwForceOnA =
                    attrs.Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
                    ((h + Ra + Rb) /
                     ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
                      (h2 + twoRah + twoRbh + 4 * Ra * Rb) * (h2 + twoRah + twoRbh + 4 * Ra * Rb))) *
                    rVecab.normalized();

                // Elastic force:
                const vec3 elasticForceOnA = -k * overlap * .5 * (rVecab / dist);

                // Gravity force:
                const vec3 gravForceOnA = (attrs.G * m[A] * m[B] / (dist * dist)) * (rVecab / dist);

                // Sliding and Rolling Friction:
                vec3 slideForceOnA{0, 0, 0};
                vec3 rollForceA{0, 0, 0};
                vec3 torqueA{0, 0, 0};
                vec3 torqueB{0, 0, 0};

                // Shared terms:
                const double elastic_force_A_mag = elasticForceOnA.norm();
                const vec3 r_a = rVecab * R[A] / sumRaRb;  // Center to contact point
                const vec3 r_b = rVecba * R[B] / sumRaRb;
                const vec3 w_diff = w[A] - w[B];

                // Sliding friction terms:
                const vec3 d_vel = vel[B] - vel[A];
                const vec3 frame_A_vel_B = d_vel - d_vel.dot(rVecab) * (rVecab / (dist * dist)) -
                                           w[A].cross(r_a) - w[B].cross(r_a);

                // Compute sliding friction force:
                const double rel_vel_mag = frame_A_vel_B.norm();
                if (rel_vel_mag > 1e-13)  // Divide by zero protection.
                {
                    // In the frame of A, B applies force in the direction of B's velocity.
                    slideForceOnA = attrs.u_s * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);
                }

                // Compute rolling friction force:
                const double w_diff_mag = w_diff.norm();
                if (w_diff_mag > 1e-13)  // Divide by zero protection.
                {
                    rollForceA =
                        -attrs.u_r * elastic_force_A_mag * (w_diff).cross(r_a) / (w_diff).cross(r_a).norm();
                }

                // Total forces on a:
                totalForceOnA = gravForceOnA + elasticForceOnA + slideForceOnA + vdwForceOnA;

                // Total torque a and b:
                torqueA = r_a.cross(slideForceOnA + rollForceA);
                torqueB = r_b.cross(-slideForceOnA + rollForceA);

                aacc[A] += torqueA / moi[A];
                aacc[B] += torqueB / moi[B];


                // No factor of 1/2. Includes both spheres:
                // PE += -G * m[A] * m[B] / dist + 0.5 * k * overlap * overlap;

                // Van Der Waals + elastic:
                const double diffRaRb = R[A] - R[B];
                const double z = sumRaRb + h;
                const double two_RaRb = 2 * R[A] * R[B];
                const double denom_sum = z * z - (sumRaRb * sumRaRb);
                const double denom_diff = z * z - (diffRaRb * diffRaRb);
                const double U_vdw =
                    -attrs.Ha / 6 *
                    (two_RaRb / denom_sum + two_RaRb / denom_diff + log(denom_sum / denom_diff));
                PE += U_vdw + 0.5 * k * overlap * overlap;

            } else  // Non-contact forces:
            {
                // No collision: Include gravity and vdw:
                // const vec3 gravForceOnA = (G * m[A] * m[B] / (dist * dist)) * (rVecab / dist);

                // Cohesion (non-contact) h must be positive or h + Ra + Rb becomes catastrophic
                // cancellation:
                double h = std::fabs(overlap);
                if (h < attrs.h_min)  // If h is closer to 0 (almost touching), use hmin.
                {
                    h = attrs.h_min;
                }
                const double Ra = R[A];
                const double Rb = R[B];
                const double h2 = h * h;
                const double twoRah = 2 * Ra * h;
                const double twoRbh = 2 * Rb * h;
                const vec3 vdwForceOnA =
                    attrs.Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
                    ((h + Ra + Rb) /
                     ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
                      (h2 + twoRah + twoRbh + 4 * Ra * Rb) * (h2 + twoRah + twoRbh + 4 * Ra * Rb))) *
                    rVecab.normalized();

                totalForceOnA = vdwForceOnA;  // +gravForceOnA;

                // PE += -G * m[A] * m[B] / dist; // Gravitational

                const double diffRaRb = R[A] - R[B];
                const double z = sumRaRb + h;
                const double two_RaRb = 2 * R[A] * R[B];
                const double denom_sum = z * z - (sumRaRb * sumRaRb);
                const double denom_diff = z * z - (diffRaRb * diffRaRb);
                const double U_vdw =
                    -attrs.Ha / 6 *
                    (two_RaRb / denom_sum + two_RaRb / denom_diff + log(denom_sum / denom_diff));
                PE += U_vdw;  // Van Der Waals


                // todo this is part of push_apart. Not great like this.
                // For pushing apart overlappers:
                // vel[A] = { 0,0,0 };
                // vel[B] = { 0,0,0 };
            }

            // Newton's equal and opposite forces applied to acceleration of each ball:
            acc[A] += totalForceOnA / m[A];
            acc[B] -= totalForceOnA / m[B];



            // So last distance can be known for COR:
            distances[e] = dist;
        }
        // DONT DO ANYTHING HERE. A STARTS AT 1.
    }

    // Calc energy:
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        KE += .5 * m[Ball] * vel[Ball].dot(vel[Ball]) + .5 * moi[Ball] * w[Ball].dot(w[Ball]);
        mom += m[Ball] * vel[Ball];
        ang_mom += m[Ball] * pos[Ball].cross(vel[Ball]) + moi[Ball] * w[Ball];
    }

}

[[nodiscard]] double Ball_group::getRmin()
{
    attrs.r_min = R[0];
    for (int Ball = 1; Ball < attrs.num_particles; Ball++) {
        if (R[Ball] < attrs.r_min) { attrs.r_min = R[Ball]; }
    }
    return attrs.r_min;
}

[[nodiscard]] double Ball_group::getRmax()
{
    attrs.r_max = R[0];
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        if (R[Ball] > attrs.r_max) { attrs.r_max = R[Ball]; }
    }
    return attrs.r_max;
}

[[nodiscard]] double Ball_group::getMmin() const
{
    double m_min = m[0];
    for (int Ball = 1; Ball < attrs.num_particles; Ball++) {
        if (m[Ball] < m_min) { m_min = m[Ball]; }
    }
    return m_min;
}

[[nodiscard]] double Ball_group::getMmax() const
{
    double m_max = m[0];
    for (int Ball = 1; Ball < attrs.num_particles; Ball++) {
        if (m[Ball] > m_max) { m_max = m[Ball]; }
    }
    return m_max;
}





void Ball_group::parseSimData(std::string line)
{
    std::string lineElement;
    // Get number of balls in file
    int count = 54 / attrs.properties;
    attrs.num_particles = static_cast<int>((static_cast<int>(std::count(line.begin(), line.end(), ',')) + 1)/11);
    if (attrs.num_particles > 0)
    {
        count = attrs.num_particles;
    }
    if (attrs.properties == -1)
    {
        std::cerr<<"WARNING: attrs.properties not set. Setting attrs.properties = 11 by default."<<std::endl;
        attrs.properties = 11;
    }
    // int count = std::count(line.begin(), line.end(), ',') / properties + 1;
    allocate_group(count);
    std::stringstream chosenLine(line);  // This is the last line of the read file, containing all data
                                         // for all balls at last time step
    

    // Get position and angular velocity data:
    for (int A = 0; A < attrs.num_particles; A++) {
        for (int i = 0; i < 3; i++)  // Position
        {
            std::getline(chosenLine, lineElement, ',');
            pos[A][i] = std::stod(lineElement);
        }
        for (int i = 0; i < 3; i++)  // Angular Velocity
        {
            std::getline(chosenLine, lineElement, ',');
            w[A][i] = std::stod(lineElement);
        }
        std::getline(chosenLine, lineElement, ',');  // Angular velocity magnitude skipped
        for (int i = 0; i < 3; i++)                  // velocity
        {
            std::getline(chosenLine, lineElement, ',');
            vel[A][i] = std::stod(lineElement);
        }
        for (int i = 0; i < attrs.properties - 10; i++)  // We used 10 elements. This skips the rest.
        {
            std::getline(chosenLine, lineElement, ',');
        }
    }
}

/// Get previous sim constants by filename.
void Ball_group::loadConsts(const std::string& path, const std::string& filename)
{
    // Get radius, mass, moi:
    std::string constantsFilename = path + filename + "constants.csv";
    if (auto ConstStream = std::ifstream(constantsFilename, std::ifstream::in)) {
        std::string line, lineElement;
        for (int A = 0; A < attrs.num_particles; A++) {
            std::getline(ConstStream, line);  // Ball line.
            std::stringstream chosenLine(line);
            std::getline(chosenLine, lineElement, ',');  // Radius.
            R[A] = std::stod(lineElement);
            std::getline(chosenLine, lineElement, ',');  // Mass.
            m[A] = std::stod(lineElement);
            std::getline(chosenLine, lineElement, ',');  // Moment of inertia.
            moi[A] = std::stod(lineElement);
        }
    } else {
        MPIsafe_print(std::cerr,"Could not open constants file: " + constantsFilename + " ... Exiting program.\n");
        MPIsafe_exit(EXIT_FAILURE);
    }
    getMass();
}


//This used to be [[nodiscard]] static std::string ... but wont compile outside the actual class definition
/// Get last line of previous simData by filename.
[[nodiscard]] std::string Ball_group::getLastLine(const std::string& path, const std::string& filename)
{


    if (auto simDataStream = std::ifstream(path+filename, std::ifstream::in)) {
        MPIsafe_print(std::cerr,"\nParsing last line of data.\n");

        simDataStream.seekg(-1, std::ios_base::end);  // go to 
         // spot before the EOF

        bool keepLooping = true;
        bool first_run = true;
        while (keepLooping) {
            char ch = ' ';
            simDataStream.get(ch);  // Get current byte's data

            if (static_cast<int>(simDataStream.tellg()) <=
                1) {                     // If the data was at or before the 0th byte
                simDataStream.seekg(0);  // The first line is the last line
                keepLooping = false;     // So stop there
            } else if (ch == '\n' && not first_run) {     // If the data was a newline
                keepLooping = false;     // Stop at the current position (if we arent on the first character).
            } else {                     // If the data was neither a newline nor at the 0 byte
                simDataStream.seekg(-2, std::ios_base::cur);  // Move to the front of that data, then to
                                                              // the front of the data before it
            }
            first_run = false;
        }
        std::string line;
        std::getline(simDataStream, line);  // Read the current line
        return line;
    } else {

        std::string message("Could not open simData file: "+path+filename+"... Exiting program.\n");
        MPIsafe_print(std::cerr,message);
        MPIsafe_exit(EXIT_FAILURE);
        return "ERROR"; //This shouldn't return but not returning anything is giving a warning
    }
}





double Ball_group::getMass()
{
    attrs.m_total = 0;
    {
        for (int Ball = 0; Ball < attrs.num_particles; Ball++) { attrs.m_total += m[Ball]; }
    }
    return attrs.m_total;
}

void Ball_group::threeSizeSphere(const int nBalls)
{
    // Make nBalls of 3 sizes in CGS with ratios such that the mass is distributed evenly among the 3
    // sizes (less large nBalls than small nBalls).
    const int smalls = static_cast<int>(std::round(
        static_cast<double>(nBalls) * 27. /
        31.375));  // Just here for reference. Whatever nBalls are left will be smalls.
    const int mediums = static_cast<int>(std::round(static_cast<double>(nBalls) * 27. / (8 * 31.375)));
    const int larges = static_cast<int>(std::round(static_cast<double>(nBalls) * 1. / 31.375));


    for (int Ball = 0; Ball < larges; Ball++) {
        // Below comment maintains asteroid radius while increasing particle count.
        // std::pow(1. / (double)nBalls, 1. / 3.) * 3. * scaleBalls;

        R[Ball] = 3. * attrs.scaleBalls;
        m[Ball] = attrs.density * 4. / 3. * 3.14159 * std::pow(R[Ball], 3);
        moi[Ball] = .4 * m[Ball] * R[Ball] * R[Ball];
        w[Ball] = {0, 0, 0};
        pos[Ball] = rand_vec3(attrs.spaceRange);
    }

    for (int Ball = larges; Ball < (larges + mediums); Ball++) {
        R[Ball] = 2. * attrs.scaleBalls;  // std::pow(1. / (double)nBalls, 1. / 3.) * 2. * scaleBalls;
        m[Ball] = attrs.density * 4. / 3. * 3.14159 * std::pow(R[Ball], 3);
        moi[Ball] = .4 * m[Ball] * R[Ball] * R[Ball];
        w[Ball] = {0, 0, 0};
        pos[Ball] = rand_vec3(attrs.spaceRange);
    }
    for (int Ball = (larges + mediums); Ball < nBalls; Ball++) {
        R[Ball] = 1. * attrs.scaleBalls;  // std::pow(1. / (double)nBalls, 1. / 3.) * 1. * scaleBalls;
        m[Ball] = attrs.density * 4. / 3. * 3.14159 * std::pow(R[Ball], 3);
        moi[Ball] = .4 * m[Ball] * R[Ball] * R[Ball];
        w[Ball] = {0, 0, 0};
        pos[Ball] = rand_vec3(attrs.spaceRange);
    }

    attrs.m_total = 0;
    for (int i = 0; i < nBalls; i++)
    {
        attrs.m_total += m[i];
        std::cerr<<"Ball "<<i<<"\tmass is "<<m[i]<<"\t"<<"radius is "<<R[i]<<std::endl;
    }

    std::cerr << "Smalls: " << smalls << " Mediums: " << mediums << " Larges: " << larges << '\n';

    // Generate non-overlapping spherical particle field:
    int collisionDetected = 0;
    int oldCollisions = nBalls;

    for (int failed = 0; failed < attrs.attempts; failed++) {
        for (int A = 0; A < nBalls; A++) {
            for (int B = A + 1; B < nBalls; B++) {
                // Check for Ball overlap.
                const double dist = (pos[A] - pos[B]).norm();
                const double sumRaRb = R[A] + R[B];
                const double overlap = dist - sumRaRb;
                if (overlap < 0) {
                    collisionDetected += 1;
                    // Move the other ball:
                    pos[B] = rand_vec3(attrs.spaceRange);
                }
            }
        }
        if (collisionDetected < oldCollisions) {
            oldCollisions = collisionDetected;
            std::cerr << "Collisions: " << collisionDetected << "                        \r";
        }
        if (collisionDetected == 0) {
            std::cerr << "\nSuccess!\n";
            break;
        }
        if (failed == attrs.attempts - 1 ||
            collisionDetected >
                static_cast<int>(
                    1.5 *
                    static_cast<double>(
                        nBalls)))  // Added the second part to speed up spatial constraint increase when
                                   // there are clearly too many collisions for the space to be feasible.
        {
            std::cerr << "Failed " << attrs.spaceRange << ". Increasing range " << attrs.spaceRangeIncrement
                      << "cm^3.\n";
            attrs.spaceRange += attrs.spaceRangeIncrement;
            failed = 0;
            for (int Ball = 0; Ball < nBalls; Ball++) {
                pos[Ball] = rand_vec3(
                    attrs.spaceRange);  // Each time we fail and increase range, redistribute all balls randomly
                                  // so we don't end up with big balls near mid and small balls outside.
            }
        }
        collisionDetected = 0;
    }

    std::cerr << "Final spacerange: " << dToSci(attrs.spaceRange) << '\n';
    std::cerr << "m_total: " << dToSci(attrs.m_total) << '\n';
    std::cerr << "Initial Radius: " << dToSci(getRadius(getCOM())) << '\n';
    std::cerr << "Mass: " << dToSci(getMass()) << '\n';
}

void Ball_group::generate_ball_field(const int nBalls)
{
    MPIsafe_print(std::cerr,"CLUSTER FORMATION (with "+std::to_string(nBalls)+" balls)\n");

    allocate_group(nBalls);

    // Create new random number set.
        //This should be d
         // in parse_input_file
    // const int seedSave = static_cast<int>(time(nullptr));
    // srand(seedSave);
    if (attrs.radiiDistribution == constant)
    {
        oneSizeSphere(nBalls);
    }
    else
    {
        distSizeSphere(nBalls);
    }
    
    calc_helpfuls();
    // threeSizeSphere(nBalls);

    attrs.output_prefix = std::to_string(nBalls) + "_R" + scientific(getRadius(getCOM())) + "_v" +
                    scientific(attrs.v_custom) + "_cor" + rounder(sqrtf(attrs.cor), 4) + "_mu" + rounder(attrs.u_s, 3) +
                    "_rho" + rounder(attrs.density, 4);
}

/// Make ballGroup from file data.
void Ball_group::loadSim(const std::string& path, const std::string& filename)
{
    std::string file = filename;
    //file we are loading is csv file
    size_t _pos;
    int file_index;

    if (file.substr(file.size()-4,file.size()) == ".csv")
    {

        _pos = file.find_first_of("_");
        size_t _lastpos = file.find_last_of("_");
        
        file_index = stoi(file.substr(0,_pos));
        
        //If this is false, then its the old way of naming
        if (_pos == _lastpos)
        {
            file = filename;
        }
        else
        {
            file = std::to_string(file_index) + file.substr(_pos,_lastpos-(_pos-1));
        }
        attrs.start_index = file_index+1;//shouldnt be file_index-1 because that is just the one we read, we will write to the next index

        std::string simFile;
        if (filename.substr(filename.length()-11,filename.length()) != "simData.csv")
        {
            simFile = filename + "simData.csv";
        }
        else
        {
            simFile = filename;
        }

        std::string constFile = simFile.substr(0,filename.length()-11);
        // if (filename.substr(filename.length()-11,filename.length()) != "simData.csv")
        // {
        //     constFile = filename + "constants.csv";
        // }
        // else
        // {
        //     constFile = filename.substr(0,filename.length()-11) + "constants.csv";
        // }



        parseSimData(getLastLine(path, simFile));
        loadConsts(path, constFile);
    }
    else if (file.substr(file.size()-3,file.size()) == ".h5")
    {
        #ifdef HDF5_ENABLE
            _pos = file.find_first_of("_");
            file_index = stoi(file.substr(0,_pos));
            loadDatafromH5(path,file);
        #else
            MPIsafe_print(std::cerr,"ERROR: HDF5 not enabled, could not open file '"+path+file+"'. Please recompile with -DHDF5_ENABLE and try again.\n");
            MPIsafe_exit(EXIT_FAILURE);
        #endif
    }
    else
    {
        MPIsafe_print(std::cerr,"ERROR: filename in loadSim is of unknown type.\n");
        MPIsafe_exit(EXIT_FAILURE);
    }

    calc_helpfuls();

    std::string message("Balls: " + std::to_string(attrs.num_particles) + '\n' + 
                        "Mass: " + dToSci(attrs.m_total) + '\n' +
                        "Approximate radius: " + dToSci(attrs.initial_radius) + " cm.\n" +
                        "SOC: " + dToSci(attrs.soc) + '\n');
    MPIsafe_print(std::cerr,message);
}

void Ball_group::parse_meta_data(std::string metadata)
{
    std::string subdata,data_t,intstr;
    size_t comma_pos,colon_pos;
    bool run = true;    

    while (run)
    {
        comma_pos = metadata.find_first_of(",");      
        subdata = metadata.substr(0,comma_pos);
        colon_pos = subdata.find_first_of(":");

        data_t = subdata.substr(0,colon_pos);
        intstr = subdata.substr(colon_pos+1,subdata.length());

        if (data_t == "steps")
        {
            attrs.steps = stoi(intstr);
        }
        else if (data_t == "skip")
        {
            attrs.skip = stoi(intstr);
        }
        else if (data_t == "kin")
        {
            std::istringstream in_stream(intstr);
            double retrieved_double;
            in_stream >> retrieved_double;
            attrs.kin = retrieved_double;
        }
        else if (data_t == "kout")
        {
            std::istringstream in_stream(intstr);
            double retrieved_double;
            in_stream >> retrieved_double;
            attrs.kout = retrieved_double;
        }
        else if (data_t == "dt")
        {
            std::istringstream in_stream(intstr);
            double retrieved_double;
            in_stream >> retrieved_double;
            attrs.dt = retrieved_double;
        }
        else
        {
            std::cerr<<"DECCO ERROR: sim metadata '"<<data_t<<"' doesn't exist."<<std::endl;
            exit(EXIT_FAILURE);
        }


        metadata = metadata.substr(comma_pos+1,metadata.length());

        if (comma_pos == std::string::npos)
        {
            run = false;
        }
    }

}




#ifdef HDF5_ENABLE
void Ball_group::loadDatafromH5(std::string path,std::string file)
{
    //read metadata to determine steps and skip variables
    std::string meta = HDF5Handler::readMetadataFromDataset("constants",path+file,attrs.sim_meta_data_name);
    size_t _pos = file.find_first_of("_");
    int file_index = stoi(file.substr(0,_pos));
    bool has_meta = true;
    //If this error happens then then we cannot restart from midway through a sim.
    //This is because the metadata containing the info needed was missing for somereason
  
    if (meta == ERR_RET_MET)  
    {
        has_meta = false;
        //If the highest sim is not finished, we need to load up the previous one and delete the partially completed sim
        if (!HDF5Handler::sim_finished(path,file))
        {
            std::string rmfile = file;

            #ifdef MPI_ENABLE
                MPI_Barrier(MPI_COMM_WORLD);
                
                int status;
                int send_result;
                //If multiple nodes, we don't want to delete until everyone has loaded
                if (getRank() == 0)
                {
                    status = remove(rmfile.c_str());
                    if (getSize() > 1)
                    {
                        for (int i = 1; i < getSize(); i++)
                        {
                            send_result = MPI_Send(&status, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                            if (send_result != MPI_SUCCESS)
                            {
                                std::cerr<<"ERROR: MPI_Send to node "<<i<<" errored with code "<<send_result<<std::endl;   
                                MPIsafe_exit(-1);
                            }
                        }

                    }
                }
                else
                {
                    MPI_Status mpistat;
                    MPI_Recv(&status, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &mpistat);
                    //verify Recv worked
                    if (mpistat.MPI_ERROR != MPI_SUCCESS)
                    {
                        std::cerr<<"ERROR: MPI_Recv for node "<<getRank()<<" errored with code "<<mpistat.MPI_ERROR<<std::endl;   
                        MPIsafe_exit(-1);
                    }
                }
            #else
                int status = remove(rmfile.c_str());
            #endif
            

            if (status != 0)
            {
                std::string message("File: '"+rmfile+"' could not be removed, now exiting with failure.\n");
                MPIsafe_print(std::cerr,message);
                MPIsafe_exit(EXIT_FAILURE);
            }
            file_index--;
            file = std::to_string(file_index) + file.substr(_pos,file.size());

        }
    }

    //This needs to be here because its used in the following function
    attrs.start_index = file_index;
    
    allocate_group(HDF5Handler::get_num_particles(path,file));
    
    //Load constants because this can be done without an initialized instance of DECCOData
    HDF5Handler::loadConsts(path,file,R,m,moi);

    int writes;
    if (attrs.typeSim != relax && has_meta) //Relax jobs should not read in the metadata for dt, steps, etc. That is for restarting jobs.
    {
        parse_meta_data(meta);

        //Now we have all info we need to initialze an instance of DECCOData.
        //However, data_written_so_far needs to be determined and set since this is a restart.
        //All this happens in the next two functions. 
        init_data(attrs.start_index);
        //writes is 0 if there is no writes so far (I don't think this should happen but if it does, more stuff needs to happen).
        //writes is >0 then that is how many writes there have been.
        //writes is -1 if there are writes and the sim is already finished. 
        writes = data->setWrittenSoFar(path,file);
    }
    else
    {

        // init_data(attrs.start_index);
        writes = -1;
    }
    // if (writes == 0)//This should really never happen. If it did then there is an empty h5 file
    // {
    //     std::cerr<<"not implimented"<<std::endl;
    //     exit(-1);
    // }
    if(writes > 0) //Works
    {
        //This cannot be done without an instance of DECCOData, that is why these are different than loadConsts
        data -> loadSimData(path,file,pos,w,vel);


        //initiate buffers since we won't call sim_init_write on a restart
        energyBuffer = std::vector<double> (data->getWidth("energy")*bufferlines);
        ballBuffer = std::vector<double> (data->getWidth("simData")*bufferlines);
        
        MPIsafe_print(std::cerr,"mid_sim_restart\n");
        attrs.mid_sim_restart = true;
        attrs.start_step = attrs.skip*(writes-1)+1;
        attrs.start_index++;
    }
    else if(writes == -1) //Works
    {
        if (attrs.typeSim != relax && has_meta)
        {
            data -> loadSimData(path,file,pos,w,vel);
        }
        else
        {
            HDF5Handler::loadh5SimData(path,file,pos,w,vel);
        }
        attrs.start_index++;
    }
    else
    {
        MPIsafe_print(std::cerr,"ERROR: in setWrittenSoFar() output of value '"+std::to_string(writes)+"'.\n");
        MPIsafe_exit(EXIT_FAILURE);
    }
     
}
#endif

void Ball_group::distSizeSphere(const int nBalls)
{
    for (int Ball = 0; Ball < nBalls; Ball++) {
        R[Ball] = lognorm_dist(attrs.scaleBalls*std::exp(-5*std::pow(attrs.lnSigma,2)/2),attrs.lnSigma);
        m[Ball] = attrs.density * 4. / 3. * 3.14159 * std::pow(R[Ball], 3);
        moi[Ball] = .4 * m[Ball] * R[Ball] * R[Ball];
        w[Ball] = {0, 0, 0};
        pos[Ball] = rand_vec3(attrs.spaceRange);
    }

    attrs.m_total = getMass();

    placeBalls(nBalls);
}

//Gives the projectile and target a velocity based on v_custom such that they collide
//at a speed of v_custom but the velocity of the center of mass is zero. The direction
//of the velocities is both down the x-axis at eachother. Also positions
//the projectile such that any offsets are taken into account and the projectile is 
//placed down the x-axis a bit. 
void Ball_group::pos_and_vel_for_collision(Ball_group &projectile)
{
    pos_and_vel_for_collision(projectile,*this);
}
void Ball_group::pos_and_vel_for_collision(Ball_group &projectile,Ball_group &target)
{
    target.attrs.initial_radius = target.getRadius(target.getCOM());

    // Collision velocity calculation:
    // Make it so collision velocity is v_custom and 
    // the velocity of the center of mass is zero
    const double mSmall = projectile.attrs.m_total;
    const double mBig = target.attrs.m_total;
    const double mTot = mBig + mSmall;
    // const double vSmall = -sqrt(2 * KEfactor * fabs(PEsys) * (mBig / (mSmall * mTot))); // Negative
    // because small offsets right.
    const double vBig = attrs.v_custom*(mSmall)/(mTot);     //-(mSmall / mBig) * vSmall;  // Negative to oppose projectile.
    const double vSmall = (vBig-attrs.v_custom);  
    // const double vBig = 0; // Dymorphous override.

    if (std::isnan(vSmall) || std::isnan(vBig)) {
        MPIsafe_print(std::cerr,"A VELOCITY WAS NAN!!!!!!!!!!!!!!!!!!!!!!\n\n");
        MPIsafe_exit(EXIT_FAILURE);
    }

    vec3 projectile_direction;
    if (attrs.typeSim == BCCA || attrs.typeSim == BPCA || attrs.typeSim == BAPA)
    {
        projectile_direction = rand_unit_vec3();
        // projectile_direction = vec3(-1,0,0);
    }
    else
    {
        projectile_direction = vec3(-1,0,0);
    }

    MPIsafe_print(std::cerr,"Projectile direction: ("+dToSci(projectile_direction.x)+','+dToSci(projectile_direction.y)+','+dToSci(projectile_direction.z)+")\n");

    projectile.kick(vSmall*projectile_direction);
    target.kick(vBig*projectile_direction);
    

    //move projectile so it is down the x-axis 
    // projectile.move(vec3(projectile.attrs.initial_radius + projectile.getRmax()*2 + target.attrs.initial_radius + target.getRmax() * 2, 0, 0));

    //This takes care of offsetting the target and projectile, but still need to move them apart
    // std::cerr<<"impact parameter: "<<attrs.impactParameter<<std::endl;

    if (attrs.impactParameter < 0.0)
    {
        //move the projectile so it is barely not touching the target
        // projectile.offset(
        //     projectile.attrs.initial_radius + projectile.getRmax(), target.attrs.initial_radius + target.getRmax(), 0);

        // std::cerr<<"projectile.init pos: "<<projectile.pos[0]<<std::endl;
        // std::cerr<<"target.init pos: "<<target.getCOM()<<std::endl;
        // std::cerr<<"projectile.attrs.initial_radius: "<<projectile.attrs.initial_radius<<std::endl;
        // std::cerr<<"projectile.R[0]: "<<projectile.R[0]<<std::endl;
        // std::cerr<<"ptarget.attrs.initial_radius: "<<target.attrs.initial_radius<<std::endl;
        // std::cerr<<"updated ptarget.attrs.initial_radius: "<<target.getRadius(target.getCOM())<<std::endl;

        // std::cerr<<"direction norm: "<<projectile_direction.norm()<<std::endl;

        // std::cerr<<"projectile target dist pre move: "<<(projectile.getCOM()-target.getCOM()).norm()<<std::endl;
        
        projectile.move((projectile.attrs.initial_radius + target.attrs.initial_radius)*projectile_direction);


        // std::cerr<<"projectile target dist post move: "<<(projectile.getCOM()-target.getCOM()).norm()<<std::endl;



        //give the projectile a random offset such that they still collide
        const auto offset = random_offset(projectile, target); 
        // std::cerr<<"projectile target dist post offset: "<<(projectile.getCOM()-target.getCOM()).norm()<<std::endl;
        MPIsafe_print(std::cerr,"Applying random offset of "+vToSci(offset)+" cm.\n");

    }
    else
    {
        // TODO::MAke this work in any direction, not just xy plane  
        MPIsafe_print(std::cerr,"ERROR::if you made it here, you need to finish this code before this can run.");
        MPIsafe_exit(-1);
        // projectile.move((projectile.attrs.initial_radius + target.attrs.initial_radius)*projectile_direction);
        projectile.offset(
            projectile.attrs.initial_radius + projectile.getRmax(), target.attrs.initial_radius + target.getRmax(), attrs.impactParameter);
        MPIsafe_print(std::cerr,"Applying impact parameter of "+std::to_string(attrs.impactParameter)+" cm.\n");
            // //Next line was the original
            // projectile.attrs.initial_radius, target.attrs.initial_radius + target.getRmax() * 2, attrs.impactParameter);
            // (projectile.attrs.initial_radius + target.attrs.initial_radius)*3, 0.0, attrs.impactParameter);
    }
    //Now we can move the aggregates apart a little bit if they are touching
    //If they are touching, move the projectile in projectile_direction
    moveApart(projectile_direction,projectile,target);
    // std::cerr<<"ptarget.attrs.initial_radius: "<<target.attrs.initial_radius<<std::endl;
    // std::cerr<<"projectile target dist at end of pos and vel for collision: "<<(projectile.getCOM()-target.getCOM()).norm()<<std::endl;
    // std::cerr<<"projectile at end of pos and vel for collision: "<<projectile.pos[0]<<std::endl;

    // MPIsafe_exit(-1);
}

void Ball_group::overwrite_v_custom(Ball_group &projectile)
{
    overwrite_v_custom(projectile,*this);
}
void Ball_group::overwrite_v_custom(Ball_group &projectile,Ball_group &target)
{

    if (attrs.eta > 0)
    {
        attrs.v_custom = calc_eta_velocity(attrs.eta,projectile,target);
        std::string message("v_custom set to "+std::to_string(attrs.v_custom)+ "cm/s based on an eta of "+
            std::to_string(attrs.eta)+".\n"); 
        MPIsafe_print(std::cerr,message);

        if (attrs.temp > 0)
        {
            message = "WARNING: Both attrs.eta and attrs.temp are greater than 0. Defaulting to setting attrs.v_custom based on eta (ignoring temperature).\n";
            MPIsafe_print(std::cerr,message);
        }
    }
    else if (attrs.temp > 0)
    {
        attrs.v_custom = calc_max_bolt_velocity(attrs.temp,projectile.getMass());
        std::string message("v_custom set to "+dToSci(attrs.v_custom)+ "cm/s based on a temp of "+
            dToSci(attrs.temp)+" degrees K and a mass of "+dToSci(projectile.getMass())+" grams.\n"); 
        MPIsafe_print(std::cerr,message);
    }
    else if (attrs.v_custom < 0)
    {
        MPIsafe_print(std::cerr,"ERROR: eta, temp, and v_custom were not specified. One of these should be positive.\n");
    }
    else
    {
        MPIsafe_print(std::cerr,"v_custom has a value of "+std::to_string(attrs.v_custom)+"cm/s based on the value in the input file.\n");
    }
}

void Ball_group::oneSizeSphere(const int nBalls)
{
    for (int Ball = 0; Ball < nBalls; Ball++) {
        R[Ball] = attrs.scaleBalls;
        m[Ball] = attrs.density * 4. / 3. * 3.14159 * std::pow(R[Ball], 3);
        moi[Ball] = .4 * m[Ball] * R[Ball] * R[Ball];
        w[Ball] = {0, 0, 0};
        pos[Ball] = rand_vec3(attrs.spaceRange);
        ////////////////////////////
        // if (Ball < nBalls-1)
        // {
        //     inout[Ball] = 0.0;
        //     distB3[Ball] = 0.0;
        // }
        // slidDir[Ball] = {0,0,0};
        // rollDir[Ball] = {0,0,0};
        // slidB3[Ball] = {0,0,0};
        // rollB3[Ball] = {0,0,0};
        // slidFric[Ball] = {0,0,0};
        // rollFric[Ball] = {0,0,0};
        ////////////////////////////
    }

    attrs.m_total = getMass();

    placeBalls(nBalls);
}

void Ball_group::placeBalls(const int nBalls)
{
    // Generate non-overlapping spherical particle field:
    int collisionDetected = 0;
    int oldCollisions = nBalls;

    if (nBalls == 1)
    {
        pos[0] = {0,1e-5,0};
    }

    for (int failed = 0; failed < attrs.attempts; failed++) {
        for (int A = 0; A < nBalls; A++) {
            for (int B = A + 1; B < nBalls; B++) {
                // Check for Ball overlap.
                const double dist = (pos[A] - pos[B]).norm();
                const double sumRaRb = R[A] + R[B];
                const double overlap = dist - sumRaRb;
                if (overlap < 0) {
                    collisionDetected += 1;
                    // Move the other ball:
                    pos[B] = rand_vec3(attrs.spaceRange);
                }
            }
        }
        if (collisionDetected < oldCollisions) {
            oldCollisions = collisionDetected;
            MPIsafe_print(std::cerr,"Collisions: "+std::to_string(collisionDetected)+'\n');
        }
        if (collisionDetected == 0) {
            MPIsafe_print(std::cerr,"Success!\n");
            break;
        }
        if (failed == attrs.attempts - 1 ||
            collisionDetected >
                static_cast<int>(
                    1.5 *
                    static_cast<double>(
                        nBalls)))  // Added the second part to speed up spatial constraint increase when
                                   // there are clearly too many collisions for the space to be feasible.
        {

            std::string message("Failed "+std::to_string(attrs.spaceRange)+". Increasing range "+std::to_string(attrs.spaceRangeIncrement)+"cm^3.\n");
            MPIsafe_print(std::cerr,message);
            attrs.spaceRange += attrs.spaceRangeIncrement;
            failed = 0;
            for (int Ball = 0; Ball < nBalls; Ball++) {
                pos[Ball] = rand_vec3(
                    attrs.spaceRange);  // Each time we fail and increase range, redistribute all balls randomly
                                  // so we don't end up with big balls near mid and small balls outside.
            }
        }
        collisionDetected = 0;
    }

    std::string message("Final spacerange: " + dToSci(attrs.spaceRange)+'\n' +
                        "Initial Radius: "+dToSci(getRadius(getCOM()))+'\n' +
                        "Mass: "+dToSci(attrs.m_total)+'\n');
    MPIsafe_print(std::cerr,message);
}




void Ball_group::updateDTK(const double& velocity)
{
    std::string initMessage = "Setting dt and k based on a velocity of "+dToSci(velocity)+" cm/s\n";
    MPIsafe_print(std::cerr,initMessage);
    calc_helpfuls();
    attrs.kin = attrs.kConsts * attrs.r_max * velocity * velocity;
    attrs.kout = attrs.cor * attrs.kin;
    const double h2 = attrs.h_min * attrs.h_min;
    // const double four_R_min = 4 * attrs.r_min * attrs.h_min;
    // const double vdw_force_max = attrs.Ha / 6 * 64 * attrs.r_min * attrs.r_min * attrs.r_min * attrs.r_min * attrs.r_min * attrs.r_min *
    //                              ((attrs.h_min + attrs.r_min + attrs.r_min) / ((h2 + four_R_min) * (h2 + four_R_min) *
    //                                                          (h2 + four_R_min + 4 * attrs.r_min * attrs.r_min) *
    //                                                          (h2 + four_R_min + 4 * attrs.r_min * attrs.r_min)));

    const double twoRminh = 2 * attrs.r_min * attrs.h_min;
    const double twoRmaxh = 2 * attrs.r_max * attrs.h_min;
    const double vdw_force_max = attrs.Ha / 6 * 64 * attrs.r_max * attrs.r_max * attrs.r_max * attrs.r_min * attrs.r_min * attrs.r_min *
                                 ((attrs.h_min + attrs.r_max + attrs.r_min) / ((h2 + twoRmaxh + twoRminh) * (h2 + twoRmaxh + twoRminh) *
                                                             ((h2 + twoRmaxh + twoRminh) + 4 * attrs.r_max * attrs.r_min) *
                                                             ((h2 + twoRmaxh + twoRminh) + 4 * attrs.r_max * attrs.r_min)));

    // const double four_R_max = 4 * attrs.r_max * attrs.h_min;
    // const double vdw_force_max2 = attrs.Ha / 6 * 64 * attrs.r_max * attrs.r_max * attrs.r_max * attrs.r_max * attrs.r_max * attrs.r_max *
    //                              ((attrs.h_min + attrs.r_max + attrs.r_max) / ((h2 + four_R_max) * (h2 + four_R_max) *
    //                                                          (h2 + four_R_max + 4 * attrs.r_max * attrs.r_max) *
    //                                                          (h2 + four_R_max + 4 * attrs.r_max * attrs.r_max)));
    // std::cerr<<"vdw: "<<vdw_force_max/getMmin()<<std::endl;
    // std::cerr<<"vdw1: "<<vdw_force_max1/getMmin()<<std::endl;
    // std::cerr<<"vdw2: "<<vdw_force_max2/getMmax()<<std::endl;

    // todo is it rmin*rmin or rmin*rmax
    const double elastic_force_max = attrs.kin * attrs.maxOverlap * attrs.r_min;
    const double regime = (vdw_force_max > elastic_force_max) ? vdw_force_max : elastic_force_max;
    const double regime_adjust = regime / (attrs.maxOverlap * attrs.r_min);

    // dt = .02 * sqrt((fourThirdsPiRho / regime_adjust) * r_min * r_min * r_min);
    attrs.dt = .01 * sqrt((attrs.fourThirdsPiRho / regime_adjust) * attrs.r_min * attrs.r_min * attrs.r_min); //NORMAL ONE
    // dt = .005 * sqrt((fourThirdsPiRho / regime_adjust) * r_min * r_min * r_min);
    std::stringstream message;
    message << "==================" << '\n';
    message << "dt set to: " << attrs.dt << '\n';
    message << "kin set to: " << attrs.kin << '\n';
    message << "kout set to: " << attrs.kout << '\n';
    message << "h_min set to: " << attrs.h_min << '\n';
    message << "Ha set to: " << attrs.Ha << '\n';
    message << "u_s set to: " << attrs.u_s << '\n';
    message << "u_r set to: " << attrs.u_r << '\n';
    if (vdw_force_max > elastic_force_max)
    {
        message << "In the vdw regime.\n";
    }
    else
    {
        message << "In the elastic regime.\n";
    }
    message << "==================" << std::endl;
    MPIsafe_print(std::cerr,message.str());
}


void Ball_group::simInit_cond_and_center(bool add_prefix)
{
    std::string message("==================\ndt: "
                        + dToSci(attrs.dt) + '\n'
                        + "k : " + dToSci(attrs.kin) + '\n'
                        + "Skip: " + std::to_string(attrs.skip) + '\n'
                        + "Steps: " + std::to_string(attrs.steps) + '\n'
                        + "==================\n");

    MPIsafe_print(std::cerr,message);

    if (attrs.num_particles > 1)
    {
        to_origin();
    }

    calc_momentum("After Zeroing");  // Is total mom zero like it should be?

    // Compute physics between all balls. Distances, collision forces, energy totals, total mass:
    init_conditions();

    // Name the file based on info above:
    if (add_prefix)
    {   
        attrs.output_prefix += "_k" + scientific(attrs.kin) + "_Ha" + scientific(attrs.Ha) + "_dt" + scientific(attrs.dt) + "_";
    }
}





// Set's up a two cluster collision.
void Ball_group::sim_init_two_cluster(
    const std::string& path,
    const std::string& projectileName,
    const std::string& targetName)
{
    // Load file data:
    std::string message("TWO CLUSTER SIM\nFile 1: " + projectileName + "\nFile 2: " + targetName + '\n');
    MPIsafe_print(std::cerr,message);
    // DART PROBE
    // ballGroup projectile(1);
    // projectile.pos[0] = { 8814, 0, 0 };
    // projectile.w[0] = { 0, 0, 0 };
    // projectile.vel[0] = { 0, 0, 0 };
    // projectile.R[0] = 78.5;
    // projectile.m[0] = 560000;
    // projectile.moi[0] = .4 * projectile.m[0] * projectile.R[0] * projectile.R[0];

    std::string projectilePath = projectileName.substr(0,projectileName.find_last_of('/')+1);
    std::string targetPath = targetName.substr(0,targetName.find_last_of('/')+1);
    std::string pName = projectileName.substr(projectileName.find_last_of('/')+1,projectileName.length());
    std::string tName = targetName.substr(targetName.find_last_of('/')+1,targetName.length());

    Ball_group projectile;
    projectile.parse_input_file(projectilePath);
    projectile.loadSim(projectilePath, pName);
    projectile.calc_v_collapse();
    projectile.to_origin();

    Ball_group target;
    target.parse_input_file(targetPath);
    target.loadSim(targetPath, tName);
    target.calc_v_collapse();
    target.to_origin();

    attrs.num_particles = projectile.attrs.num_particles + target.attrs.num_particles;
    
    //Update v_custom if temp or eta are greater than zero
    overwrite_v_custom(projectile,target);
    
    MPIsafe_print(std::cerr,"Total number of particles in sim: "+std::to_string(attrs.num_particles) + '\n');

    // DO YOU WANT TO STOP EVERYTHING?
    projectile.zeroAngVel();
    projectile.zeroVel();
    target.zeroAngVel();
    target.zeroVel();

    pos_and_vel_for_collision(projectile,target);

    // //move projectile so it is down the x-axis 
    // projectile.move(vec3(projectile.attrs.initial_radius + projectile.getRmax()*2 + target.attrs.initial_radius + target.getRmax() * 2, 0, 0));

    // //This takes care of offsetting the target and projectile, but still need to move them apart
    // projectile.offset(
    //     projectile.attrs.initial_radius + projectile.getRmax()*2, target.attrs.initial_radius + target.getRmax() * 2, attrs.impactParameter);
    //     //Next line was the original
    //     // projectile.attrs.initial_radius, target.attrs.initial_radius + target.getRmax() * 2, attrs.impactParameter);
    //     // (projectile.attrs.initial_radius + target.attrs.initial_radius)*3, 0.0, attrs.impactParameter);

    // //Now we can move the aggregates apart
    // projectile.move(vec3((projectile.attrs.initial_radius + projectile.getRmax()*2 + target.attrs.initial_radius + target.getRmax() * 2)*2, 0, 0));


    // // Collision velocity calculation:
    // // Make it so collision velocity is v_custom and 
    // // the velocity of the center of mass is zero
    // const double mSmall = projectile.attrs.m_total;
    // const double mBig = target.attrs.m_total;
    // const double mTot = mBig + mSmall;
    // // const double vSmall = -sqrt(2 * KEfactor * fabs(PEsys) * (mBig / (mSmall * mTot))); // Negative
    // // because small offsets right.
    // const double vBig = attrs.v_custom*(mSmall)/(mTot);     //-(mSmall / mBig) * vSmall;  // Negative to oppose projectile.
    // const double vSmall = (vBig-attrs.v_custom);                
    // // const double vBig = 0; // Dymorphous override.

    // if (std::isnan(vSmall) || std::isnan(vBig)) {
    //     MPIsafe_print(std::cerr,"A VELOCITY WAS NAN!!!!!!!!!!!!!!!!!!!!!!\n\n");
    //     MPIsafe_exit(EXIT_FAILURE);
    // }

    // projectile.kick(vec3(vSmall, 0, 0));
    // target.kick(vec3(vBig, 0, 0));

    // std::ostringstream oss;
    // oss << "\nTarget Velocity: " << std::scientific << vBig
    //     << "\nProjectile Velocity: " << vSmall << "\n\n";
    // MPIsafe_print(std::cerr,oss.str());
    //This is jobs line. Keeping it in case this new printing fails
    // fprintf(message, "\nTarget Velocity: %.2e\nProjectile Velocity: %.2e\n", vBig, vSmall);

    projectile.calc_momentum("Projectile");
    target.calc_momentum("Target");

    allocate_group(projectile.attrs.num_particles + target.attrs.num_particles);

    // double new_v_collapse = (projectile.attrs.v_collapse >= target.attrs.v_collapse) ? projectile.attrs.v_collapse : target.attrs.v_collapse;

    merge_ball_group(target);
    merge_ball_group(projectile);  // projectile second so smallest ball at end and largest ball at front
                                   // for dt/k calcs.
    // attrs.v_collapse = new_v_collapse;

    //move center of mass to origin
    to_origin();

    // attrs.output_prefix = projectileName + targetName + "T" + rounder(attrs.KEfactor, 4) + "_vBig" +
    //                 scientific(vBig) + "_vSmall" + scientific(vSmall) + "_IP" +
    //                 rounder(attrs.impactParameter * 180 / 3.14159, 2) + "_rho" + rounder(attrs.density, 4);
}

// @brief checks if this is new job or restart.
// @returns 0 if this is starting from scratch
// @returns 1 if this is a restart
// @returns 2 if this job is already finished
int Ball_group::check_restart(std::string folder) 
{
    std::string file;
    // int tot_count = 0;
    // int file_count = 0;
    int largest_file_index = -1;
    int file_index=0;
    for (const auto & entry : fs::directory_iterator(folder))
    {
        if (!fs::is_regular_file(entry.path())) {
            continue;  // Skip directories or non-regular files
        }

        file = entry.path().filename().string();
        
        if (file.substr(0,file.size()-4) == "timing")
        {
            return 2;
        }

        //Is the data in csv format? (and isnt job_data.csv)
        if (file.size() >= 4 && file.substr(file.size()-4,file.size()) == ".csv" && file != "job_data.csv")
        {
            // file_count++;
            size_t _pos = file.find_first_of("_");
            size_t _secpos = file.substr(_pos+1,file.size()).find_first_of("_");
            _secpos += _pos+1; //add 1 to account for _pos+1 in substr above
            file_index = stoi(file.substr(0,file.find_first_of("_")));
            if (file[_pos+1] == 'R')
            {
                file_index = 0;
            }

            if (file_index > largest_file_index)
            {
                largest_file_index = file_index;
            }
        }
        else if (file.size() >= 3 && file.substr(file.size()-3,file.size()) == ".h5")
        {
            size_t _pos = file.find_first_of("_");
            file_index = stoi(file.substr(0,file.find_first_of("_")));
            if (file_index > largest_file_index)
            {
                largest_file_index = file_index;
            }
        }
    }
    
    if (largest_file_index > -1)
    {
        return 1;
    }
    else
    {
        return 0;
    }

    
}

//with a known slope and intercept, givin N, the number of particles, what is the 
//optimum number of threads. The function then chooses the power of 2 that is closest
//to this optimum
int Ball_group::get_num_threads()
{
    // int N = attrs.num_particles;
    // //This is from speed tests on COSINE
    // double slope = ;
    // double intercept = ;

    // double interpolatedValue = slope * n + intercept; // Linear interpolation
    // return std::min(closestPowerOf2(interpolatedValue),attrs.MAXOMPthreads);        // Find the closest power of 2

    //I could only test up to 16 threads so far. Not enough data for linear interp
    

    int threads;
    // if (N < 0)
    // {
    //     std::cerr<<"ERROR: negative number of particles."<<std::endl;
    //     exit(-1);
    // }
    // else if (N < 80)
    // {
    //     threads = 1;
    // }
    // else if (N < 100)
    // {
    //     threads = 2;
    // }
    // else
    // {
    //     threads = 16;
    // }

    // if (threads > attrs.MAXOMPthreads)
    // {
        threads = attrs.MAXOMPthreads;
    // }
    return threads;
}


std::string Ball_group::find_file_name(std::string path,int index)
{
    std::string file;
    const std::string simDatacsv = "simData.csv";
    const std::string datah5 = "data.h5";
    int file_index;


    for (const auto & entry : fs::directory_iterator(path))
    {

        file = entry.path();
        size_t slash_pos = file.find_last_of("/");
        file = file.erase(0,slash_pos+1);
        size_t _pos = file.find_first_of("_");

        if (_pos != std::string::npos) // only go in here if we found a data file
        {
            //Is the data in csv format? (but first verify the call to substr wont fail)
            if (file.size() >= simDatacsv.size() && file.substr(file.size()-simDatacsv.size(),file.size()) == simDatacsv)
            {
                int num_ = std::count(file.begin(),file.end(),'_');
                file_index = stoi(file.substr(0,file.find_first_of("_")));
                if (num_ > 1) // old name convention
                {
                    if (index == 0)
                    {
                        if (file[_pos+1] == 'R')
                        {
                            return path+file;
                        }
                    }
                    else if (index > 0)
                    {
                        if (file_index == index)
                        {
                            return path+file;
                        }
                    }
                }
                else if (num_ == 1) // new name convention ONLY TESTED WITH THIS CASE
                {
                    if (file_index == index)
                    {
                        return path+file;
                    }
                }
                else
                {
                    MPIsafe_print(std::cerr,"ERROR: filename convention is not recognized for file '"+file+"'\n");
                    exit(-1);
                }

            }
            else if (file.size() >= datah5.size() && file.substr(file.size()-datah5.size(),file.size()) == datah5)
            {
                file_index = stoi(file.substr(0,file.find_first_of("_")));
                if (file_index == index)
                {
                    return path+file;
                }
            }
        }
    }
    
    MPIsafe_print(std::cerr,"ERROR: file at path '"+path+"' with index '"+std::to_string(index)+"' not found. Now exiting . . .\n");
    exit(-1);
}


//Returns the path + filename of the specified index
//If index < 0 (default is -1) then it will return the largest (completed) index
std::string Ball_group::find_whole_file_name(std::string path, const int index)
{
    std::string file;
    std::string largest_file_name;
    std::string second_largest_file_name;
    std::string simDatacsv = "simData.csv";
    std::string datah5 = "data.h5";

    // TODO::: write this function so we actually know if its csv or h5
    std::string dataType = data_type_from_input(path);
    int csv = -1;
    if (dataType == "csv")
    {
        csv = 1;
    }
    else if (dataType == "h5" || dataType == "hdf5")
    {
        csv = 0;
    }
    else
    {
        std::string test_file = path+std::to_string(index);
        if (fs::exists(test_file+"_simData.csv"))
        {
            return test_file + "_simData.csv";
        }
        else if (fs::exists(test_file+"_data.h5"))
        {
            return test_file + "_data.h5";
        }
        else if (fs::exists(test_file+"_data.hdf5"))
        {
            return test_file + "_data.hdf5";
        }
        else
        {
            MPIsafe_print(std::cerr,"ERROR in find_whole_file_name: dataType '"+dataType+"' not recognized.");
            MPIsafe_exit(-1);
        }
    }

    int largest_file_index = -1;
    int second_largest_file_index = -1;
    int file_index=0;
    for (const auto & entry : fs::directory_iterator(path))
    {
        file = entry.path();
        size_t pos = file.find_last_of("/");
        file = file.erase(0,pos+1);

        //Is the data in csv format?
        //If so, find largest and second largest file index
        // if (file.size() > simDatacsv.size() && csv)
        if (file.size() > simDatacsv.size() && file.substr(file.size()-simDatacsv.size(),file.size()) == simDatacsv)
        {
            // file_count++;
            size_t _pos = file.find_first_of("_");
            size_t _secpos = file.substr(_pos+1,file.size()).find_first_of("_");
            _secpos += _pos+1; //add 1 to account for _pos+1 in substr above
            file_index = stoi(file.substr(0,file.find_first_of("_")));
            if (file[_pos+1] == 'R')
            {
                file_index = 0;
            }
            if (file_index > largest_file_index)
            {
                second_largest_file_index = largest_file_index;
                second_largest_file_name = largest_file_name;
                largest_file_index = file_index;
                largest_file_name = file;
            }
            else if (file_index > second_largest_file_index)
            {
                second_largest_file_name = file;
                second_largest_file_index = file_index;
            }



        }
        // else if (file.size() > datah5.size() && not csv)
        else if (file.size() > datah5.size() && file.substr(file.size()-datah5.size(),file.size()) == datah5)
        {
            size_t _pos = file.find_first_of("_");
            file_index = stoi(file.substr(0,file.find_first_of("_")));
            if (file_index > largest_file_index)
            {
                largest_file_index = file_index;
                largest_file_name = file;
            }
        }
    }



    //if index is greater than zero we know if its csv or h5 so just return here
    if (index >= 0)
    {
        if (csv == 1)
        {
            return std::to_string(index) + "_" + simDatacsv;
        }
        else
        {
            return std::to_string(index) + "_" + datah5;
        }
    }


    //TODO: check if {index}_checkpoint.txt exists don't delete, that sim is complete
    bool checkpoint_exists = fs::exists(path+std::to_string(largest_file_index)+"_checkpoint.txt");
    if (csv == 1 && index < 0 && second_largest_file_index > 0 && !checkpoint_exists)
    {
        if (getRank() == 0)
        {
            std::string file1 = path + largest_file_name;
            std::string file2 = path + largest_file_name.substr(0,largest_file_name.size()-simDatacsv.size()) + "constants.csv";
            std::string file3 = path + largest_file_name.substr(0,largest_file_name.size()-simDatacsv.size()) + "energy.csv";
            // std::string file4 = path + largest_file_name.substr(0,largest_file_name.size()-simDatacsv.size()) + "checkpoint.txt";

            std::string message("Removing the following files: \n"
                                +'\t'+file1+'\n'
                                +'\t'+file2+'\n'
                                +'\t'+file3+'\n');
                                // +'\t'+file4+'\n');
            MPIsafe_print(std::cerr,message);

            int status1 = remove(file1.c_str());
            int status2 = remove(file2.c_str());
            int status3 = remove(file3.c_str());
            // int status4 = remove(file4.c_str());


            if (status1 != 0)
            {
                MPIsafe_print(std::cerr,"File1: '"+file1+"' could not be removed, now exiting with failure.\n");
                MPIsafe_exit(EXIT_FAILURE);
            }
            else if (status2 != 0)
            {
                MPIsafe_print(std::cerr,"File2: '"+file2+"' could not be removed, now exiting with failure.\n");
                MPIsafe_exit(EXIT_FAILURE);
            }
            else if (status3 != 0)
            {
                MPIsafe_print(std::cerr,"File3: '"+file3+"' could not be removed, now exiting with failure.\n");
                MPIsafe_exit(EXIT_FAILURE);
            }
            // else if (status4 != 0)
            // {
            //     MPIsafe_print(std::cerr,"File4: '"+file4+"' could not be removed, now exiting with failure.\n");
            //     MPIsafe_exit(EXIT_FAILURE);
            // }
        }
        MPIsafe_barrier();
        largest_file_name = second_largest_file_name;
    }
    return largest_file_name;
}

#ifndef GPU_ENABLE
void Ball_group::sim_one_step(int step,bool write_step)
{
    int world_rank = getRank();
    int world_size = getSize();
    /// FIRST PASS - Update Kinematic Parameters:
    // t.start_event("UpdateKinPar");
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        // Update velocity half step:
        velh[Ball] = vel[Ball] + .5 * acc[Ball] * attrs.dt;

        // Update angular velocity half step:
        wh[Ball] = w[Ball] + .5 * aacc[Ball] * attrs.dt;

        // Update position:
        pos[Ball] += velh[Ball] * attrs.dt;

        // Reinitialize acceleration to be recalculated:
        acc[Ball] = {0, 0, 0};

        // Reinitialize angular acceleration to be recalculated:
        aacc[Ball] = {0, 0, 0};
    }
    // t.end_event("UpdateKinPar");

    double Ha = attrs.Ha;
    double u_r = attrs.u_r;
    double u_s = attrs.u_s;
    double kin = attrs.kin;
    double kout = attrs.kout;
    double h_min = attrs.h_min;
    double dt = attrs.dt;
    int num_parts = attrs.num_particles;
    int threads = attrs.OMPthreads;
    bool write_step = attrs.write_step;

    
    long long A;
    long long B;
    long long pc;
    long long lllen = attrs.num_particles;
    double t0 = omp_get_wtime();
    #pragma omp declare reduction(vec3_sum : vec3 : omp_out += omp_in)
    #pragma omp parallel for num_threads(threads)\
            reduction(vec3_sum:acc[:num_parts],aacc[:num_parts]) reduction(+:PE) \
            shared(world_rank,world_size,Ha,write_step,lllen,R,pos,vel,m,w,\
                u_r,u_s,moi,kin,kout,distances,h_min,dt)\
            default(none) private(A,B,pc) 
    for (pc = world_rank + 1; pc <= (((lllen*lllen)-lllen)/2); pc += world_size)
    {
        long double pd = (long double)pc;
        pd = (sqrt(pd*8.0L+1.0L)+1.0L)*0.5L;
        pd -= 0.00001L;
        A = (long long)pd;
        B = (long long)((long double)pc-(long double)A*((long double)A-1.0L)*.5L-1.0L);

 
        const double sumRaRb = R[A] + R[B];
        const vec3 rVecab = pos[B] - pos[A];  // Vector from a to b.
        const vec3 rVecba = -rVecab;
        const double dist = (rVecab).norm();

        //////////////////////
        // const double grav_scale = 3.0e21;
        //////////////////////

        // Check for collision between Ball and otherBall:
        double overlap = sumRaRb - dist;

        

        vec3 totalForceOnA{0, 0, 0};

        // Distance array element: 1,0    2,0    2,1    3,0    3,1    3,2 ...
        int e = static_cast<unsigned>(A * (A - 1) * .5) + B;  // a^2-a is always even, so this works.
        double oldDist = distances[e];
        /////////////////////////////
        // double inoutT;
        /////////////////////////////
        // Check for collision between Ball and otherBall.
        if (overlap > 0) {



            double k;
            if (dist >= oldDist) {
                k = kout;
            } else {
                k = kin;
            }

            // Cohesion (in contact) h must always be h_min:
            // constexpr double h = h_min;
            const double h = h_min;
            const double Ra = R[A];
            const double Rb = R[B];
            const double h2 = h * h;
            // constexpr double h2 = h * h;
            const double twoRah = 2 * Ra * h;
            const double twoRbh = 2 * Rb * h;

            // const vec3 vdwForceOnA = Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
            //                              ((h + Ra + Rb) / ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
            //                                                (h2 + twoRah + twoRbh + 4 * Ra * Rb) *
            //                                                (h2 + twoRah + twoRbh + 4 * Ra * Rb))) *
            //                              rVecab.normalized();

            // ==========================================
            // Test new vdw force equation with less division
            const double d1 = h2 + twoRah + twoRbh;
            const double d2 = d1 + 4 * Ra * Rb;
            const double numer = 64*Ha*Ra*Ra*Ra*Rb*Rb*Rb*(h+Ra+Rb);
            const double denomrecip = 1/(6*d1*d1*d2*d2);
            const vec3 vdwForceOnA = (numer*denomrecip)*rVecab.normalized();
            // ==========================================

            // Elastic force:
            // vec3 elasticForceOnA{0, 0, 0};
            // if (std::fabs(overlap) > 1e-6)
            // {
            //     elasticForceOnA = -k * overlap * .5 * (rVecab / dist);
            // }
            const vec3 elasticForceOnA = -k * overlap * .5 * (rVecab / dist);
            ///////////////////////////////
            // elasticForce[A] += elasticForceOnA;
            // elasticForce[B] -= elasticForceOnA;
            ///////////////////////////////
            ///////////////////////////////
            ///////material parameters for silicate composite from Reissl 2023
            // const double Estar = 1e5*169; //in Pa
            // const double nu2 = 0.27*0.27; // nu squared (unitless)
            // const double prevoverlap = sumRaRb - oldDist;
            // const double rij = sqrt(std::pow(Ra,2)-std::pow((Ra-overlap/2),2));
            // const double Tvis = 15e-12; //Viscoelastic timescale (15ps)
            // // const double Tvis = 5e-12; //Viscoelastic timescale (5ps)
            // const vec3 viscoelaticforceOnA = -(2*Estar/nu2) * 
            //                                  ((overlap - prevoverlap)/dt) * 
            //                                  rij * Tvis * (rVecab / dist);
            const vec3 viscoelaticforceOnA = {0,0,0};
            ///////////////////////////////

            // Gravity force:
            // const vec3 gravForceOnA = (G * m[A] * m[B] * grav_scale / (dist * dist)) * (rVecab / dist); //SCALE MASS
            const vec3 gravForceOnA = {0,0,0};
            // const vec3 gravForceOnA = (G * m[A] * m[B] / (dist * dist)) * (rVecab / dist);

            // Sliding and Rolling Friction:
            vec3 slideForceOnA{0, 0, 0};
            vec3 rollForceA{0, 0, 0};
            vec3 torqueA{0, 0, 0};
            vec3 torqueB{0, 0, 0};

            // Shared terms:
            const double elastic_force_A_mag = elasticForceOnA.norm();
            const vec3 r_a = rVecab * R[A] / sumRaRb;  // Center to contact point
            const vec3 r_b = rVecba * R[B] / sumRaRb;
            const vec3 w_diff = w[A] - w[B];

            // Sliding friction terms:
            const vec3 d_vel = vel[B] - vel[A];
            const vec3 frame_A_vel_B = d_vel - d_vel.dot(rVecab) * (rVecab / (dist * dist)) -
                                       w[A].cross(r_a) - w[B].cross(r_a);

            // Compute sliding friction force:
            const double rel_vel_mag = frame_A_vel_B.norm();

            ////////////////////////////////////////// CALC THIS AT INITIALIZATION for all combos os Ra,Rb
            // const double u_scale = calc_VDW_force_mag(Ra,Rb,h_min_physical)/
            //                         vdwForceOnA.norm();         //Friction coefficient scale factor
            //////////////////////////////////////////
            if (rel_vel_mag > 1e-13)  // NORMAL ONE Divide by zero protection.
            {
                // slideForceOnA = u_s * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);
                // In the frame of A, B applies force in the direction of B's velocity.
                ///////////////////////////////////
                // if (mu_scale)
                // {
                //     if (u_scale[e]*u_s > max_mu)
                //     {
                //         slideForceOnA = max_mu * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);
                //     }
                //     else
                //     {
                //         slideForceOnA = u_scale[e] * u_s * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);
                //     }
                // }
                // else
                // {
                    slideForceOnA = u_s * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);
                // }
                ///////////////////////////////////
            }
            //////////////////////////////////////
            // slideForce[A] += slideForceOnA;
            // slideForce[B] -= slideForceOnA;
            //////////////////////////////////////


            // Compute rolling friction force:
            const double w_diff_mag = w_diff.norm();
            // if (w_diff_mag > 1e-20)  // Divide by zero protection.
            // if (w_diff_mag > 1e-8)  // Divide by zero protection.
            if (w_diff_mag > 1e-13)  // NORMAL ONE Divide by zero protection.
            {
                // rollForceA = 
                //     -u_r * elastic_force_A_mag * (w_diff).cross(r_a) / 
                //     (w_diff).cross(r_a).norm();
                /////////////////////////////////////
                // if (mu_scale)
                // {
                //     if (u_scale[e]*u_r > max_mu)
                //     {
                //         rollForceA = 
                //             -max_mu * elastic_force_A_mag * (w_diff).cross(r_a) / 
                //             (w_diff).cross(r_a).norm();
                //     }
                //     else
                //     {
                //         rollForceA = 
                //             -u_scale[e] * u_r * elastic_force_A_mag * (w_diff).cross(r_a) / 
                //             (w_diff).cross(r_a).norm();
                //     }
                // }
                // else
                // {
                    rollForceA = 
                        -u_r * elastic_force_A_mag * (w_diff).cross(r_a) / 
                        (w_diff).cross(r_a).norm();
                // }
                /////////////////////////////////////
            }


            // Total forces on a:
            // totalForceOnA = gravForceOnA + elasticForceOnA + slideForceOnA + vdwForceOnA;
            ////////////////////////////////
            totalForceOnA = viscoelaticforceOnA + gravForceOnA + elasticForceOnA + slideForceOnA + vdwForceOnA;
            ////////////////////////////////

            

            // Total torque a and b:
            torqueA = r_a.cross(slideForceOnA + rollForceA);
            torqueB = r_b.cross(-slideForceOnA + rollForceA); // original code



            aacc[A] += torqueA / moi[A];
            aacc[B] += torqueB / moi[B];

            if (write_step) {
                // No factor of 1/2. Includes both spheres:
                // PE += -G * m[A] * m[B] * grav_scale / dist + 0.5 * k * overlap * overlap;
                // PE += -G * m[A] * m[B] / dist + 0.5 * k * overlap * overlap;

                // Van Der Waals + elastic:
                const double diffRaRb = R[A] - R[B];
                const double z = sumRaRb + h;
                const double two_RaRb = 2 * R[A] * R[B];
                const double denom_sum = z * z - (sumRaRb * sumRaRb);
                const double denom_diff = z * z - (diffRaRb * diffRaRb);
                const double U_vdw =
                    -Ha / 6 *
                    (two_RaRb / denom_sum + two_RaRb / denom_diff + 
                    log(denom_sum / denom_diff));
                PE += U_vdw + 0.5 * k * overlap * overlap; ///TURN ON FOR REAL SIM
            }
        } else  // Non-contact forces:
        {

            // No collision: Include gravity and vdw:
            // const vec3 gravForceOnA = (G * m[A] * m[B] * grav_scale / (dist * dist)) * (rVecab / dist);
            const vec3 gravForceOnA = {0.0,0.0,0.0};
            // Cohesion (non-contact) h must be positive or h + Ra + Rb becomes catastrophic cancellation:
            double h = std::fabs(overlap);
            if (h < h_min)  // If h is closer to 0 (almost touching), use hmin.
            {
                h = h_min;
            }
            const double Ra = R[A];
            const double Rb = R[B];
            const double h2 = h * h;
            const double twoRah = 2 * Ra * h;
            const double twoRbh = 2 * Rb * h;

            // const vec3 vdwForceOnA = Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
            //                              ((h + Ra + Rb) / ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
            //                                                (h2 + twoRah + twoRbh + 4 * Ra * Rb) *
            //                                                (h2 + twoRah + twoRbh + 4 * Ra * Rb))) *
            //                              rVecab.normalized();
            // ==========================================
            // Test new vdw force equation with less division
            const double d1 = h2 + twoRah + twoRbh;
            const double d2 = d1 + 4 * Ra * Rb;
            const double numer = 64*Ha*Ra*Ra*Ra*Rb*Rb*Rb*(h+Ra+Rb);
            const double denomrecip = 1/(6*d1*d1*d2*d2);
            const vec3 vdwForceOnA = (numer*denomrecip)*rVecab.normalized();
            // ==========================================
           
            /////////////////////////////
            totalForceOnA = vdwForceOnA + gravForceOnA;
            // totalForceOnA = vdwForceOnA;
            // totalForceOnA = gravForceOnA;
            /////////////////////////////
            if (write_step) {
                // PE += -G * m[A] * m[B] * grav_scale / dist; // Gravitational

                const double diffRaRb = R[A] - R[B];
                const double z = sumRaRb + h;
                const double two_RaRb = 2 * R[A] * R[B];
                const double denom_sum = z * z - (sumRaRb * sumRaRb);
                const double denom_diff = z * z - (diffRaRb * diffRaRb);
                const double U_vdw =
                    -Ha / 6 *
                    (two_RaRb / denom_sum + two_RaRb / denom_diff + log(denom_sum / denom_diff));
                PE += U_vdw;  // Van Der Waals TURN ON FOR REAL SIM
            }

            // todo this is part of push_apart. Not great like this.
            // For pushing apart overlappers:
            // vel[A] = { 0,0,0 };
            // vel[B] = { 0,0,0 };
        }

        // Newton's equal and opposite forces applied to acceleration of each ball:
        acc[A] += totalForceOnA / m[A];
        acc[B] -= totalForceOnA / m[B];

        


        // So last distance can be known for COR:
        distances[e] = dist;

    }

    #ifdef MPI_ENABLE
        MPI_Allreduce(MPI_IN_PLACE,acc,attrs.num_particles*3,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
        MPI_Allreduce(MPI_IN_PLACE,aacc,attrs.num_particles*3,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
        double local_PE = PE;
        PE = 0.0;
        MPI_Reduce(&local_PE,&PE,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    #endif

    // t.end_event("CalcForces/loopApplicablepairs");


    // THIRD PASS - Calculate velocity for next step:
    // t.start_event("CalcVelocityforNextStep");
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) 
    {
        // Velocity for next step:
        vel[Ball] = velh[Ball] + .5 * acc[Ball] * attrs.dt;
        w[Ball] = wh[Ball] + .5 * aacc[Ball] * attrs.dt;

        /////////////////////////////////
        // if (true) {
        /////////////////////////////////
        if (write_step && world_rank == 0) 
        {
            // Send positions and rotations to buffer:
            int start = data->getWidth("simData")*attrs.num_writes+Ball*data->getSingleWidth("simData");
            ballBuffer[start] = pos[Ball][0];
            ballBuffer[start+1] = pos[Ball][1];
            ballBuffer[start+2] = pos[Ball][2];
            ballBuffer[start+3] = w[Ball][0];
            ballBuffer[start+4] = w[Ball][1];
            ballBuffer[start+5] = w[Ball][2];
            ballBuffer[start+6] = w[Ball].norm();
            ballBuffer[start+7] = vel[Ball][0];
            ballBuffer[start+8] = vel[Ball][1];
            ballBuffer[start+9] = vel[Ball][2];
            ballBuffer[start+10] = 0;

            KE += .5 * m[Ball] * vel[Ball].normsquared() +
                    .5 * moi[Ball] * w[Ball].normsquared();  // Now includes rotational kinetic energy.
            mom += m[Ball] * vel[Ball];
            ang_mom += m[Ball] * pos[Ball].cross(vel[Ball]) + moi[Ball] * w[Ball];
        }
    }  // THIRD PASS END
    if (write_step && world_rank == 0)
    {
        attrs.num_writes ++;
    }
    // t.end_event("CalcVelocityforNextStep");
}  // one Step end
#endif 

#ifdef GPU_ENABLE
void Ball_group::sim_one_step(int step,bool write_step)
{

    #pragma acc parallel loop deviceptr(d_velh,d_vel,d_acc,d_aacc,d_wh,d_w,d_pos,d_attrs)
    for (int Ball = 0; Ball < d_attrs->num_particles; Ball++) {
        // Update velocity half step:
        d_velh[Ball] = d_vel[Ball] + .5 * d_acc[Ball] * d_attrs->dt;

        // Update angular velocity half step:
        d_wh[Ball] = d_w[Ball] + .5 * d_aacc[Ball] * d_attrs->dt;

        // Update position:
        d_pos[Ball] += d_velh[Ball] * d_attrs->dt;

        // Reinitialize acceleration to be recalculated:
        d_acc[Ball] = {0, 0, 0};

        // Reinitialize angular acceleration to be recalculated:
        d_aacc[Ball] = {0, 0, 0};
    }


    #pragma acc parallel loop deviceptr(d_accsq, d_aaccsq,d_attrs)
    for (int i = 0; i < d_attrs->num_particles*d_attrs->num_particles; ++i)
    {
        d_accsq[i] = {0.0,0.0,0.0};
        d_aaccsq[i] = {0.0,0.0,0.0};
    }
 

    int start = attrs.world_rank+1;
    int stop = attrs.num_pairs;
    int leap = attrs.world_size;


    #pragma acc parallel deviceptr(d_accsq, d_aaccsq, d_m, d_moi, d_w, d_vel, d_pos, d_R, d_distances,d_attrs)
    {

        #pragma acc loop reduction(+:PE) 
        for (int pc = start; pc <= stop; pc += leap)
        {


            double pd = (double)pc;
            pd = (sqrt(pd*8.0+1.0)+1.0)*0.5;
            pd -= 0.00001;
            int A = (int)pd;
            int B = (int)((double)pc-(double)A*((double)A-1.0)*.5-1.0);

     
            const double sumRaRb = d_R[A] + d_R[B];

            const vec3 rVecab = d_pos[B] - d_pos[A];  // Vector from a to b.
            const vec3 rVecba = -rVecab;
            const double dist = (rVecab).norm();

            //////////////////////
            // const double grav_scale = 3.0e21;
            //////////////////////

            // Check for collision between Ball and otherBall:
            double overlap = sumRaRb - dist;

            vec3 totalForceOnA{0, 0, 0};

            // Distance array element: 1,0    2,0    2,1    3,0    3,1    3,2 ...
            int e = static_cast<unsigned>(A * (A - 1) * .5) + B;  // a^2-a is always even, so this works.
            double oldDist = d_distances[e];
            
            
            // Check for collision between Ball and otherBall.
            if (overlap > 0) {


                double k;
                if (dist >= oldDist) {
                    k = d_attrs->kout;
                } else {
                    k = d_attrs->kin;
                }

                // Cohesion (in contact) h must always be h_min:
                // constexpr double h = h_min;
                const double h = d_attrs->h_min;
                const double Ra = d_R[A];
                const double Rb = d_R[B];
                const double h2 = h * h;
                // constexpr double h2 = h * h;
                const double twoRah = 2 * Ra * h;
                const double twoRbh = 2 * Rb * h;


                const double d1 = h2 + twoRah + twoRbh;
                const double d2 = d1 + 4 * Ra * Rb;
                const double numer = 64*d_attrs->Ha*Ra*Ra*Ra*Rb*Rb*Rb*(h+Ra+Rb);
                const double denomrecip = 1/(6*d1*d1*d2*d2);
                const vec3 vdwForceOnA = (numer*denomrecip)*rVecab.normalized();


                const vec3 elasticForceOnA = -k * overlap * .5 * (rVecab / dist);


                // Gravity force:
                // const vec3 gravForceOnA = (G * m[A] * m[B] * grav_scale / (dist * dist)) * (rVecab / dist); //SCALE MASS
                const vec3 gravForceOnA = {0,0,0};
                // const vec3 gravForceOnA = (G * m[A] * m[B] / (dist * dist)) * (rVecab / dist);

                // Sliding and Rolling Friction:
                vec3 slideForceOnA{0, 0, 0};
                vec3 rollForceA{0, 0, 0};
                vec3 torqueA{0, 0, 0};
                vec3 torqueB{0, 0, 0};

                // Shared terms:
                const double elastic_force_A_mag = elasticForceOnA.norm();
                const vec3 r_a = rVecab * d_R[A] / sumRaRb;  // Center to contact point
                const vec3 r_b = rVecba * d_R[B] / sumRaRb;
                const vec3 w_diff = d_w[A] - d_w[B];

                // Sliding friction terms:
                const vec3 vel_diff = d_vel[B] - d_vel[A];


                const vec3 frame_A_vel_B = vel_diff - vel_diff.dot(rVecab) * (rVecab / (dist * dist)) -
                                           d_w[A].cross(r_a) - d_w[B].cross(r_a);



                // Compute sliding friction force:
                const double rel_vel_mag = frame_A_vel_B.norm();

                if (rel_vel_mag > 1e-13)  // NORMAL ONE Divide by zero protection.
                {
                        slideForceOnA = d_attrs->u_s * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);

                }


                // Compute rolling friction force:
                const double w_diff_mag = w_diff.norm();
                if (w_diff_mag > 1e-13)  // NORMAL ONE Divide by zero protection.
                {

                        rollForceA = 
                            -d_attrs->u_r * elastic_force_A_mag * (w_diff).cross(r_a) / 
                            (w_diff).cross(r_a).norm();
                }


                // Total forces on a:
                // totalForceOnA = gravForceOnA + elasticForceOnA + slideForceOnA + vdwForceOnA;
                ////////////////////////////////
                totalForceOnA = gravForceOnA + elasticForceOnA + slideForceOnA + vdwForceOnA;
                ////////////////////////////////

                // Total torque a and b:
                torqueA = r_a.cross(slideForceOnA + rollForceA);
                torqueB = r_b.cross(-slideForceOnA + rollForceA); // original code

                vec3 aaccA = (1/d_moi[A])*torqueA;
                vec3 aaccB = (1/d_moi[B])*torqueB;

                d_aaccsq[A*d_attrs->num_particles+B].x = aaccA.x;
                d_aaccsq[A*d_attrs->num_particles+B].y = aaccA.y;
                d_aaccsq[A*d_attrs->num_particles+B].z = aaccA.z;
                d_aaccsq[B*d_attrs->num_particles+A].x = aaccB.x;
                d_aaccsq[B*d_attrs->num_particles+A].y = aaccB.y;
                d_aaccsq[B*d_attrs->num_particles+A].z = aaccB.z;

                if (write_step) {
                    // No factor of 1/2. Includes both spheres:
                    // PE += -G * m[A] * m[B] * grav_scale / dist + 0.5 * k * overlap * overlap;
                    // PE += -G * m[A] * m[B] / dist + 0.5 * k * overlap * overlap;

                    // Van Der Waals + elastic:
                    const double diffRaRb = d_R[A] - d_R[B];
                    const double z = sumRaRb + h;
                    const double two_RaRb = 2 * d_R[A] * d_R[B];
                    const double denom_sum = z * z - (sumRaRb * sumRaRb);
                    const double denom_diff = z * z - (diffRaRb * diffRaRb);
                    const double U_vdw =
                        -d_attrs->Ha / 6 *
                        (two_RaRb / denom_sum + two_RaRb / denom_diff + 
                        log(denom_sum / denom_diff));
                    PE += U_vdw + 0.5 * k * overlap * overlap; ///TURN ON FOR REAL SIM
                }
            } 
            else  // Non-contact forces:
            {

                // No collision: Include gravity and vdw:
                // const vec3 gravForceOnA = (G * m[A] * m[B] * grav_scale / (dist * dist)) * (rVecab / dist);
                const vec3 gravForceOnA = {0.0,0.0,0.0};
                // Cohesion (non-contact) h must be positive or h + Ra + Rb becomes catastrophic cancellation:
                double h = std::fabs(overlap);
                if (h < d_attrs->h_min)  // If h is closer to 0 (almost touching), use hmin.
                {
                    h = d_attrs->h_min;
                }
                const double Ra = d_R[A];
                const double Rb = d_R[B];
                const double h2 = h * h;
                const double twoRah = 2 * Ra * h;
                const double twoRbh = 2 * Rb * h;


                const double d1 = h2 + twoRah + twoRbh;
                const double d2 = d1 + 4 * Ra * Rb;
                const double numer = 64*d_attrs->Ha*Ra*Ra*Ra*Rb*Rb*Rb*(h+Ra+Rb);
                const double denomrecip = 1/(6*d1*d1*d2*d2);
                const vec3 vdwForceOnA = (numer*denomrecip)*rVecab.normalized();

                /////////////////////////////
                totalForceOnA = vdwForceOnA + gravForceOnA;
                // totalForceOnA = vdwForceOnA;
                // totalForceOnA = gravForceOnA;
                /////////////////////////////
                if (write_step) {
                    // PE += -G * m[A] * m[B] * grav_scale / dist; // Gravitational

                    const double diffRaRb = d_R[A] - d_R[B];
                    const double z = sumRaRb + h;
                    const double two_RaRb = 2 * d_R[A] * d_R[B];
                    const double denom_sum = z * z - (sumRaRb * sumRaRb);
                    const double denom_diff = z * z - (diffRaRb * diffRaRb);
                    const double U_vdw =
                        -d_attrs->Ha / 6 *
                        (two_RaRb / denom_sum + two_RaRb / denom_diff + log(denom_sum / denom_diff));
                    PE += U_vdw;  // Van Der Waals TURN ON FOR REAL SIM
                }

            }

            // Newton's equal and opposite forces applied to acceleration of each ball:
            vec3 accA = (1/d_m[A])*totalForceOnA; 
            vec3 accB = -1.0*(1/d_m[B])*totalForceOnA; 



            d_accsq[A*d_attrs->num_particles+B].x = accA.x;
            d_accsq[A*d_attrs->num_particles+B].y = accA.y;
            d_accsq[A*d_attrs->num_particles+B].z = accA.z;
            d_accsq[B*d_attrs->num_particles+A].x = accB.x;
            d_accsq[B*d_attrs->num_particles+A].y = accB.y;
            d_accsq[B*d_attrs->num_particles+A].z = accB.z;


            // So last distance can be known for COR:
            d_distances[e] = dist;

        }
    }




    #pragma acc parallel loop deviceptr(d_accsq,d_aaccsq,d_acc,d_aacc,d_attrs)
    for (int i = 0; i < d_attrs->num_particles; i++)
    {

        #pragma acc loop seq
        for (int j = 0; j < d_attrs->num_particles; j++)
        {

            d_acc[i].x += d_accsq[i*d_attrs->num_particles+j].x;
            d_acc[i].y += d_accsq[i*d_attrs->num_particles+j].y;
            d_acc[i].z += d_accsq[i*d_attrs->num_particles+j].z;
            d_aacc[i].x += d_aaccsq[i*d_attrs->num_particles+j].x;
            d_aacc[i].y += d_aaccsq[i*d_attrs->num_particles+j].y;
            d_aacc[i].z += d_aaccsq[i*d_attrs->num_particles+j].z;
        }

    }

    
    if (write_step)
    {
        #pragma acc update host(PE)
    }

    #ifdef MPI_ENABLE
        acc_memcpy_from_device(acc,d_acc, attrs.num_particles * sizeof(vec3));
        acc_memcpy_from_device(aacc,d_aacc, attrs.num_particles * sizeof(vec3));

        MPI_Allreduce(MPI_IN_PLACE,acc,attrs.num_particles*3,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
        MPI_Allreduce(MPI_IN_PLACE,aacc,attrs.num_particles*3,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
        double local_PE = PE;
        PE = 0.0;
        MPI_Reduce(&local_PE,&PE,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
        // #pragma acc update device(acc[0:attrs.num_particles],aacc[0:attrs.num_particles])

        acc_memcpy_to_device(d_acc,acc, attrs.num_particles * sizeof(vec3));
        acc_memcpy_to_device(d_aacc,aacc, attrs.num_particles * sizeof(vec3));
    #endif


    #pragma acc parallel loop deviceptr(d_velh,d_acc,d_aacc,d_w,d_vel,d_wh,d_attrs)
    for (int Ball = 0; Ball < d_attrs->num_particles; Ball++) {
        // Velocity for next step:
        d_vel[Ball] = d_velh[Ball] + .5 * d_acc[Ball] * d_attrs->dt;
        d_w[Ball] = d_wh[Ball] + .5 * d_aacc[Ball] * d_attrs->dt;
    }  // THIRD PASS END



    // THIRD PASS - Calculate velocity for next step:
    if (write_step && attrs.world_rank == 0) 
    {

        acc_memcpy_from_device(w, d_w, attrs.num_particles * sizeof(vec3));
        acc_memcpy_from_device(vel, d_vel, attrs.num_particles * sizeof(vec3));
        acc_memcpy_from_device(pos, d_pos, attrs.num_particles * sizeof(vec3));

        for (int Ball = 0; Ball < attrs.num_particles; Ball++) 
        {
            // Send positions and rotations to buffer:
            int start = data->getWidth("simData")*attrs.num_writes+Ball*data->getSingleWidth("simData");
            ballBuffer[start] = pos[Ball][0];
            ballBuffer[start+1] = pos[Ball][1];
            ballBuffer[start+2] = pos[Ball][2];
            ballBuffer[start+3] = w[Ball][0];
            ballBuffer[start+4] = w[Ball][1];
            ballBuffer[start+5] = w[Ball][2];
            ballBuffer[start+6] = w[Ball].norm();
            ballBuffer[start+7] = vel[Ball][0];
            ballBuffer[start+8] = vel[Ball][1];
            ballBuffer[start+9] = vel[Ball][2];
            ballBuffer[start+10] = 0;

            KE += .5 * m[Ball] * vel[Ball].normsquared() +
                    .5 * moi[Ball] * w[Ball].normsquared();  // Now includes rotational kinetic energy.
            mom += m[Ball] * vel[Ball];
            ang_mom += m[Ball] * pos[Ball].cross(vel[Ball]) + moi[Ball] * w[Ball];
        }
    }  // THIRD PASS END
    if (write_step && attrs.world_rank == 0)
    {
        attrs.num_writes ++;
    }


}  // one Step end
#endif

#ifndef GPU_ENABLE
void
Ball_group::sim_looper(unsigned long long start_step=1)
{


    attrs.world_rank = getRank();
    attrs.world_size = getSize();
    attrs.num_pairs = static_cast<int>(attrs.num_particles*(attrs.num_particles-1)/2);

    attrs.num_writes = 0;
    unsigned long long Step;
    // attrs.writeStep = false;

    if (attrs.world_rank == 0)
    {   
        attrs.startProgress = time(nullptr);
    }

    std::string message(
        "Beginning simulation...\nstart step:" +
        std::to_string(start_step)+'\n' +
        "Stepping through "+std::to_string(attrs.steps)+" steps.\n" + 
        "Simulating "+dToSci(attrs.simTimeSeconds)+" seconds per sim.\n" + 
        "Writing out every "+dToSci(attrs.timeResolution)+" seconds.\n" +
        "For a total of "+dToSci(attrs.simTimeSeconds/attrs.timeResolution)+" timesteps saved per sim.\n");
    MPIsafe_print(std::cerr,message);

    //Set the number of threads to be appropriate
    // #ifndef GPU_ENABLE
    attrs.OMPthreads = get_num_threads();
    // #else
    //     std::cerr<<"ADDING STUFF TO THE GPUUUUU"<<std::endl;
        // #pragma acc enter data copyin(this) 
    //     #pragma acc enter data create(accsq[0:attrs.num_particles*attrs.num_particles],aaccsq[0:attrs.num_particles*attrs.num_particles])
    //     #pragma acc enter data copyin(moi[0:attrs.num_particles],m[0:attrs.num_particles],\
    //         w[0:attrs.num_particles],vel[0:attrs.num_particles],pos[0:attrs.num_particles],R[0:attrs.num_particles],\
    //         distances[0:attrs.num_pairs]) 
    //     #pragma acc enter data copyin(acc[0:attrs.num_particles],aacc[0:attrs.num_particles],\
    //         velh[0:attrs.num_particles],wh[0:attrs.num_particles]) 
    //     #pragma acc enter data copyin(attrs.dt,attrs.num_pairs,attrs.num_particles,attrs.Ha,attrs.kin,attrs.kout,attrs.h_min,\
    //         attrs.u_s,attrs.u_r,attrs.world_rank,attrs.world_size,attrs.write_step,PE)
    //     // #pragma acc enter data copyin(accsq[0:attrs.num_particles*attrs.num_particles],\
    //     //     aaccsq[0:attrs.num_particles*attrs.num_particles],acc[0:attrs.num_particles],aacc[0:attrs.num_particles],\
    //     //     velh[0:attrs.num_particles],wh[0:attrs.num_particles]) 
    // #endif

    for (Step = start_step; Step < attrs.steps; Step++)  // Steps start at 1 for non-restart because the 0 step is initial conditions.
    {

        // simTimeElapsed += dt; //New code #1
        // Check if this is a write step:
        if (Step % attrs.skip == 0) {
            // if (world_rank == 0)
            // {
            //     t.start_event("writeProgressReport");
            // }
            attrs.write_step = true;

            // #ifdef GPU_ENABLE
            //     #pragma acc update device(attrs.write_step)
            // #endif
            // std::cerr<<"Write step "<<Step<<std::endl;

            /////////////////////// Original code #1
            attrs.simTimeElapsed += attrs.dt * attrs.skip;
            ///////////////////////

            if (attrs.world_rank == 0)
            {
                // Progress reporting:
                float eta = ((time(nullptr) - attrs.startProgress) / static_cast<float>(attrs.skip) *
                             static_cast<float>(attrs.steps - Step)) /
                            3600.f;  // Hours.
                float real = (time(nullptr) - attrs.start) / 3600.f;
                float simmed = static_cast<float>(attrs.simTimeElapsed / 3600.f);
                float progress = (static_cast<float>(Step) / static_cast<float>(attrs.steps) * 100.f);
                fprintf(
                    stderr,
                    "%llu\t%2.0f%%\tETA: %5.2lf\tReal: %5.2f\tSim: %5.2f hrs\tR/S: %5.2f\n",
                    Step,
                    progress,
                    eta,
                    real,
                    simmed,
                    real / simmed);
                // fprintf(stdout, "%u\t%2.0f%%\tETA: %5.2lf\tReal: %5.2f\tSim: %5.2f hrs\tR/S: %5.2f\n", Step,
                // progress, eta, real, simmed, real / simmed);
                fflush(stdout);
                attrs.startProgress = time(nullptr);
                // t.end_event("writeProgressReport");
            }
        } else {
            attrs.write_step = attrs.debug;
        }


        // std::cerr<<"step: "<<Step<<"\tskip: "<<attrs.skip<<std::endl;

        // Physics integration step:
        sim_one_step(Step);
        // #ifndef GPU_ENABLE
        // #else
        //     sim_one_step_GPU();
        // #endif

        if (attrs.write_step) {

            std::cerr<<"WRITING STEP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;

            if (attrs.world_rank == 0)
            {    
                int start = data->getWidth("energy")*(attrs.num_writes-1);
                energyBuffer[start] = attrs.simTimeElapsed;
                energyBuffer[start+1] = PE;
                energyBuffer[start+2] = KE;
                energyBuffer[start+3] = PE+KE;
                energyBuffer[start+4] = mom.norm();
                energyBuffer[start+5] = ang_mom.norm();

                if (Step / attrs.skip % 10 == 0) 
                {


                    std::cerr << "vMax = " << getVelMax() << "\nSteps recorded: " << Step / attrs.skip << '\n';
                    std::cerr << "Data Write to "<<data->getFileName()<<"\n";
                    
                    data->Write(ballBuffer,"simData",bufferlines);

                    ballBuffer.clear();
                    ballBuffer = std::vector<double>(data->getWidth("simData")*bufferlines);
                    data->Write(energyBuffer,"energy");
                    energyBuffer.clear();
                    energyBuffer = std::vector<double>(data->getWidth("energy")*bufferlines);

                    attrs.num_writes = 0;

                }  // Data export end
                
                attrs.lastWrite = time(nullptr);
            }
            
            // Reinitialize energies for next step:
            KE = 0;
            PE = 0;
            #ifdef GPU_ENABLE
                #pragma acc update device(PE)
            #endif
            mom = {0, 0, 0};
            ang_mom = {0, 0, 0};

            // if (attrs.dynamicTime) { calibrate_dt(Step, false); }
            // t.end_event("writeStep");
        }  // writestep end
    }

    // #ifdef GPU_ENABLE
    //     #pragma acc exit data delete(accsq[0:attrs.num_particles*attrs.num_particles],\
    //         aaccsq[0:attrs.num_particles*attrs.num_particles],acc[0:attrs.num_particles],aacc[0:attrs.num_particles])
    //     #pragma acc exit data delete(m[0:attrs.num_particles],w[0:attrs.num_particles],vel[0:attrs.num_particles],\
    //         pos[0:attrs.num_particles],R[0:attrs.num_particles],distances[0:attrs.num_pairs])
    //     #pragma acc exit data delete(attrs.dt,attrs.num_pairs,attrs.num_particles,attrs.Ha,\
    //         attrs.kin,attrs.kout,attrs.h_min,attrs.u_s,attrs.u_r,attrs.world_rank,attrs.world_size,attrs.write_step,PE)
    //     // #pragma acc exit data delete(this)
    // #endif

    //if this is an aggregation job, make sure the final state is all connected (we didnt miss the target)
    if (isAggregation())
    {
        if (!isConnected(pos,R,attrs.num_particles))
        {
            //For now just stop the sim so I can verify this isConnected works
            MPIsafe_print(std::cerr,"ERROR: aggregate failed isConnected. Now exiting. . .\n");
            MPIsafe_exit(-1);
        }
        else
        {
            MPIsafe_print(std::cerr,"Aggregate is connected.\n");
        }
    }

    if (attrs.world_rank == 0)
    {
        const time_t end = time(nullptr);

        std::cerr << "Simulation complete! \n"
                  << attrs.num_particles << " Particles and " << Step << '/' << attrs.steps << " Steps.\n"
                  << "Simulated time: " << attrs.steps * attrs.dt << " seconds\n"
                  << "Computation time: " << end - attrs.start << " seconds\n";
        std::cerr << "\n===============================================================\n";
    }

    data->write_checkpoint();
}  // end simLooper
#endif //Non GPU looper


#ifdef GPU_ENABLE
void
Ball_group::sim_looper(unsigned long long start_step=1)
{


    attrs.world_rank = getRank();
    attrs.world_size = getSize();
    attrs.num_pairs = static_cast<int>(attrs.num_particles*(attrs.num_particles-1)/2);

    attrs.num_writes = 0;
    unsigned long long Step;
    // attrs.writeStep = false;

    if (attrs.world_rank == 0)
    {   
        attrs.startProgress = time(nullptr);
    }

    std::string message(
        "Beginning simulation...\nstart step:" +
        std::to_string(start_step)+'\n' +
        "Stepping through "+std::to_string(attrs.steps)+" steps.\n" + 
        "Simulating "+dToSci(attrs.simTimeSeconds)+" seconds per sim.\n" + 
        "Writing out every "+dToSci(attrs.timeResolution)+" seconds.\n" +
        "For a total of "+dToSci(attrs.simTimeSeconds/attrs.timeResolution)+" timesteps saved per sim.\n");
    MPIsafe_print(std::cerr,message);

    //Set the number of threads to be appropriate

    //Copy the scalars to the GPU
    Device_attributes local_d_attrs;

    local_d_attrs.kout = attrs.kout;
    local_d_attrs.kin = attrs.kin;
    local_d_attrs.Ha = attrs.Ha;
    local_d_attrs.u_s = attrs.u_s;
    local_d_attrs.u_s = attrs.u_s;
    local_d_attrs.h_min = attrs.h_min;
    local_d_attrs.dt = attrs.dt;
    local_d_attrs.num_particles = attrs.num_particles;
    local_d_attrs.num_pairs = attrs.num_pairs;



    //Copy the arrays to the GPU
    d_attrs = (Device_attributes*) acc_malloc(sizeof(Device_attributes));
    d_accsq  = (vec3*) acc_malloc( attrs.num_particles*attrs.num_particles * sizeof(vec3) );
    d_aaccsq = (vec3*) acc_malloc( attrs.num_particles*attrs.num_particles * sizeof(vec3) );
    d_velh = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_vel = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_w = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_wh = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_pos = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_acc = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_aacc = (vec3*) acc_malloc( attrs.num_particles * sizeof(vec3) );
    d_moi = (double*) acc_malloc( attrs.num_particles * sizeof(double) );
    d_R = (double*) acc_malloc( attrs.num_particles * sizeof(double) );
    d_m = (double*) acc_malloc( attrs.num_particles * sizeof(double) );
    d_distances = (double*) acc_malloc( attrs.num_pairs * sizeof(double) );

    acc_memcpy_to_device(d_attrs, &local_d_attrs, sizeof(Device_attributes));
    acc_memcpy_to_device(d_velh, velh, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_vel, vel, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_w, w, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_wh, wh, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_pos, pos, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_acc, acc, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_aacc, aacc, attrs.num_particles * sizeof(vec3));
    acc_memcpy_to_device(d_moi, moi, attrs.num_particles * sizeof(double));
    acc_memcpy_to_device(d_R, R, attrs.num_particles * sizeof(double));
    acc_memcpy_to_device(d_m, m, attrs.num_particles * sizeof(double));
    acc_memcpy_to_device(d_distances, distances, attrs.num_pairs * sizeof(double));

    acc_map_data(this->d_attrs, d_attrs, sizeof(Device_attributes));
    acc_map_data(this->d_aaccsq, d_aaccsq, attrs.num_particles*attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_accsq, d_accsq, attrs.num_particles*attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_velh, velh, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_vel, d_vel, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_w, d_w, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_wh, d_wh, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_pos, d_pos, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_acc, d_acc, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_aacc, d_aacc, attrs.num_particles * sizeof(vec3));
    acc_map_data(this->d_moi, d_moi, attrs.num_particles * sizeof(double));
    acc_map_data(this->d_R, d_R, attrs.num_particles * sizeof(double));
    acc_map_data(this->d_m, d_m, attrs.num_particles * sizeof(double));
    acc_map_data(this->d_distances, d_distances, attrs.num_pairs * sizeof(double));

        // copyin(this[0:1])

    bool write_step = false;// = attrs.write_step; //Passing bools and chars as a struct member to the GPU does not play well

    #pragma acc enter data \
        copyin(this[0:1],PE)
    {

        // acc_map_data(this->velh, velh, attrs.num_particles * sizeof(vec3));

        for (Step = start_step; Step < attrs.steps; Step++)  // Steps start at 1 for non-restart because the 0 step is initial conditions.
        {

            // Check if this is a write step:
            if (Step % attrs.skip == 0) {
                write_step = true;


                /////////////////////// Original code #1
                attrs.simTimeElapsed += attrs.dt * attrs.skip;
                ///////////////////////

                if (attrs.world_rank == 0)
                {
                    // Progress reporting:
                    float eta = ((time(nullptr) - attrs.startProgress) / static_cast<float>(attrs.skip) *
                                 static_cast<float>(attrs.steps - Step)) /
                                3600.f;  // Hours.
                    float real = (time(nullptr) - attrs.start) / 3600.f;
                    float simmed = static_cast<float>(attrs.simTimeElapsed / 3600.f);
                    float progress = (static_cast<float>(Step) / static_cast<float>(attrs.steps) * 100.f);
                    fprintf(
                        stderr,
                        "%llu\t%2.0f%%\tETA: %5.2lf\tReal: %5.2f\tSim: %5.2f hrs\tR/S: %5.2f\n",
                        Step,
                        progress,
                        eta,
                        real,
                        simmed,
                        real / simmed);
                    // fprintf(stdout, "%u\t%2.0f%%\tETA: %5.2lf\tReal: %5.2f\tSim: %5.2f hrs\tR/S: %5.2f\n", Step,
                    // progress, eta, real, simmed, real / simmed);
                    fflush(stdout);
                    attrs.startProgress = time(nullptr);
                    // t.end_event("writeProgressReport");
                }
            } else {
                write_step = attrs.debug;
            }


            // std::cerr<<"step: "<<Step<<"\tskip: "<<attrs.skip<<std::endl;

            // Physics integration step:
            sim_one_step(Step,write_step);
            // #ifndef GPU_ENABLE
            // #else
            //     sim_one_step_GPU();
            // #endif

            if (write_step) {

                if (attrs.world_rank == 0)
                {    
                    int start = data->getWidth("energy")*(attrs.num_writes-1);
                    energyBuffer[start] = attrs.simTimeElapsed;
                    energyBuffer[start+1] = PE;
                    energyBuffer[start+2] = KE;
                    energyBuffer[start+3] = PE+KE;
                    energyBuffer[start+4] = mom.norm();
                    energyBuffer[start+5] = ang_mom.norm();

                    if (Step / attrs.skip % 10 == 0) 
                    {


                        std::cerr << "vMax = " << getVelMax() << "\nSteps recorded: " << Step / attrs.skip << '\n';
                        std::cerr << "Data Write to "<<data->getFileName()<<"\n";
                        
                        data->Write(ballBuffer,"simData",bufferlines);

                        ballBuffer.clear();
                        ballBuffer = std::vector<double>(data->getWidth("simData")*bufferlines);
                        data->Write(energyBuffer,"energy");
                        energyBuffer.clear();
                        energyBuffer = std::vector<double>(data->getWidth("energy")*bufferlines);

                        attrs.num_writes = 0;

                        // ///////////////////TEMPORARY STOPGAP///////////////////
                        // if (!isConnected(pos,R,attrs.num_particles))
                        // {
                        //     MPIsafe_print(std::cerr,"NOT CONNECTED AFTER WRITE\n");
                        //     MPIsafe_exit(-1);
                        // }
                        // ///////////////////TEMPORARY STOPGAP///////////////////
                    }  // Data export end
                    
                    attrs.lastWrite = time(nullptr);
                }


                // Reinitialize energies for next step:
                KE = 0;
                PE = 0;
                #ifdef GPU_ENABLE
                    #pragma acc update device(PE)
                #endif
                mom = {0, 0, 0};
                ang_mom = {0, 0, 0};

                // if (attrs.dynamicTime) { calibrate_dt(Step, false); }
                // t.end_event("writeStep");
            }  // writestep end
        }
    }


    acc_memcpy_from_device(pos, d_pos, attrs.num_particles * sizeof(vec3));
    acc_memcpy_from_device(R, d_R, attrs.num_particles * sizeof(vec3));

    acc_unmap_data(this->d_aaccsq);
    acc_unmap_data(this->d_accsq);
    acc_unmap_data(this->d_aacc);
    acc_unmap_data(this->d_acc);
    acc_unmap_data(this->d_m);
    acc_unmap_data(this->d_w);
    acc_unmap_data(this->d_wh);
    acc_unmap_data(this->d_moi);
    acc_unmap_data(this->d_vel);
    acc_unmap_data(this->d_velh);
    acc_unmap_data(this->d_pos);
    acc_unmap_data(this->d_R);
    acc_unmap_data(this->d_distances);
    acc_unmap_data(this->d_attrs);


    if (d_accsq)  acc_free(d_accsq);
    if (d_aaccsq) acc_free(d_aaccsq);
    if (d_aacc) acc_free(d_aacc);
    if (d_acc) acc_free(d_acc);
    if (d_moi) acc_free(d_moi);
    if (d_m) acc_free(d_m);
    if (d_w) acc_free(d_w);
    if (d_wh) acc_free(d_wh);
    if (d_vel) acc_free(d_vel);
    if (d_velh) acc_free(d_velh);
    if (d_pos) acc_free(d_pos);
    if (d_R) acc_free(d_R);
    if (d_distances) acc_free(d_distances);
    if (d_attrs) acc_free(d_attrs);

    // #pragma acc exit data delete(attrs.dt,attrs.num_pairs,attrs.num_particles,attrs.Ha,\
    //     attrs.kin,attrs.kout,attrs.h_min,attrs.u_s,attrs.u_r,attrs.world_rank,attrs.world_size,PE)
    #pragma acc exit data delete(this,PE)

    //if this is an aggregation job, make sure the final state is all connected (we didnt miss the target)
    if (isAggregation())
    {
        if (!isConnected(pos,R,attrs.num_particles))
        {
            //For now just stop the sim so I can verify this isConnected works
            MPIsafe_print(std::cerr,"ERROR: aggregate failed isConnected. Now exiting. . .\n");
            MPIsafe_exit(-1);
        }
        else
        {
            MPIsafe_print(std::cerr,"Aggregate is connected.\n");
        }
    }

    if (attrs.world_rank == 0)
    {
        const time_t end = time(nullptr);

        std::cerr << "Simulation complete! \n"
                  << attrs.num_particles << " Particles and " << Step << '/' << attrs.steps << " Steps.\n"
                  << "Simulated time: " << attrs.steps * attrs.dt << " seconds\n"
                  << "Computation time: " << end - attrs.start << " seconds\n";
        std::cerr << "\n===============================================================\n";
    }

    data->write_checkpoint();
}  // end simLooper
#endif //GPU looper



bool Ball_group::isAggregation()
{
    if (attrs.typeSim == BAPA || attrs.typeSim == BPCA || attrs.typeSim == BCCA)
    {
        return true;
    }
    return false;
}


//Checks if any of projectil's balls are overlapping any target balls
bool is_touching(Ball_group &projectile,Ball_group &target)
{
    for (int i = 0; i < projectile.attrs.num_particles; ++i)
    {
        for (int j = 0; j < target.attrs.num_particles; ++j)
        {
            if((projectile.R[i]+target.R[j]) > (projectile.pos[i] - target.pos[j]).norm())
            {
                return true;
            }
        }
    }
    return false;
}

void moveApart(const vec3 &projectile_direction,Ball_group &projectile,Ball_group &target)
{

    double min_r = projectile.attrs.r_min > target.attrs.r_min ? target.attrs.r_min : projectile.attrs.r_min;

    bool touching = is_touching(projectile,target);


    while (touching)
    {
        projectile.move(-min_r*projectile_direction);
        touching = is_touching(projectile,target);
    }
}