#pragma once
#include "../default_files/dust_const.hpp"
// #include "dust_const.hpp"
#include "../external/json/single_include/nlohmann/json.hpp"
#include "../utilities/vec3.hpp"
#include "../utilities/linalg.hpp"
#include "../utilities/Utils.hpp"
#include "../data/DECCOData.hpp"
#include "../timing/timing.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <limits.h>
#include <cstring>
#include <typeinfo>
#include <memory>
#include <random>
#include <omp.h>
#include <mpi.h>

// using std::numbers::pi;
using json = nlohmann::json;
namespace fs = std::filesystem;
extern const int bufferlines;

int getSize()
{
    int world_size;
    #ifdef MPI_ENABLE
        MPI_Comm_size(MPI_COMM_WORLD,&world_size);
    #else
        world_size = 0;
    #endif
    return world_size;
}

int getRank()
{
    int world_rank;
    #ifdef MPI_ENABLE
        MPI_Comm_rank(MPI_COMM_WORLD,&world_rank);
    #else
        world_rank = 0;
    #endif
    return world_rank;
}

struct Ball_group_attributes
{
    std::string project_path;
    std::string output_folder;
    std::string data_directory;
    std::string projectileName;
    std::string targetName;
    std::string output_prefix;

    double radiiFraction = -1;
    bool debug = false;
    bool write_all = false;
    bool mid_sim_restart = false;

    // std::string out_folder;
    int num_particles = 0;
    int num_particles_added = 0;
    int MAXOMPthreads = 1;
    int MAXMPInodes = 1;
    int start_index = 0;
    int start_step = 1;

    int skip=-1;  // Steps thrown away before recording a step to the buffer. 500*.04 is every 20 seconds in sim.
    int steps=-1;

    double dt=-1;
    double kin=-1;  // Spring constant
    double kout=-1;

    const std::string sim_meta_data_name = "sim_info";

    int seed = -1;
    int output_width = -1;
    enum distributions {constant, logNorm};
    distributions radiiDistribution;
    enum simType {BPCA, collider};
    simType typeSim;
    double lnSigma = 0.2; //sigma for log normal distribution 

    // Useful values:
    double r_min = -1;
    double r_max = -1;
    double m_total = -1;
    double initial_radius = -1;
    double v_collapse = 0;
    double v_max = -1;
    double v_max_prev = HUGE_VAL;
    double soc = -1;
    

    bool dynamicTime = false;
    double G;  // Gravitational constant
    double density;
    double u_s;                // Coeff of sliding friction
    double u_r;               // Coeff of rolling friction
    double sigma;              // Poisson ratio for rolling friction.
    double Y;               // Young's modulus in erg/cm3
    double cor;                // Coeff of restitution
    double simTimeSeconds;  // Seconds
    double timeResolution;    // Seconds - This is duration between exported steps.
    double fourThirdsPiRho;  // for fraction of smallest sphere radius.
    double scaleBalls;                         // base radius of ball.
    double maxOverlap;                           // of scaleBalls
    double KEfactor;                              // Determines collision velocity based on KE/PE
    double v_custom;  // Velocity cm/s
    double temp;          //tempurature of simulation in Kelvin
    double kConsts;
    double impactParameter;  // Impact angle radians
    double Ha;         // Hamaker constant for vdw force
    double h_min;  // 1e8 * std::numeric_limits<double>::epsilon(), // 2.22045e-10 (epsilon is 2.22045e-16)
    double cone;  // Cone of particles ignored moving away from center of mass. Larger angle ignores more.

    // Simulation Structure
    int properties;  // Number of columns in simData file per ball
    int genBalls;
    int attempts;  // How many times to try moving every ball touching another in generator.


    double spaceRange;  // Rough minimum space required
    double spaceRangeIncrement;
    double z0Rot;  // Cluster one z axis rotation
    double y0Rot;  // Cluster one y axis rotation
    double z1Rot;  // Cluster two z axis rotation
    double y1Rot;  // Cluster two y axis rotation
    double simTimeElapsed;

    int N=-1; //Number of balls to grow (if BPCA)

    const time_t start = time(nullptr);  // For end of program analysis
    time_t startProgress;                // For progress reporting (gets reset)
    time_t lastWrite;                    // For write control (gets reset)

    /////////////////////////////////
    const double h_min_physical = 2.1e-8; //prolly should make this a parameter/calculation
    const double max_mu = 0.5; // Make another parameter
    bool mu_scale = false;
    /////////////////////////////////
    // data_type 0 for hdf5 
    // data_type 1 for csv (not implimented) 
    int data_type = 0;
    std::string filetype = "h5";
    int num_writes = 0;


    // Overload the assignment operator
    Ball_group_attributes& operator=(const Ball_group_attributes& other) 
    {
        if (this != &other) // Protect against self-assignment 
        {  
            project_path = other.project_path;
            output_folder = other.output_folder;
            data_directory = other.data_directory;
            projectileName = other.projectileName;
            targetName = other.targetName;
            output_prefix = other.output_prefix;

            radiiFraction = other.radiiFraction;
            debug = other.debug;
            write_all = other.write_all;
            mid_sim_restart = other.mid_sim_restart;

            num_particles = other.num_particles;
            num_particles_added = other.num_particles_added;
            MAXOMPthreads = other.MAXOMPthreads;
            MAXMPInodes = other.MAXMPInodes;
            start_index = other.start_index;
            start_step = other.start_step;

            skip = other.skip;
            steps = other.steps;

            dt = other.dt;
            kin = other.kin;
            kout = other.kout;

            seed = other.seed;
            output_width = other.output_width;
            radiiDistribution = other.radiiDistribution;
            typeSim = other.typeSim;
            lnSigma = other.lnSigma;

            r_min = other.r_min;
            r_max = other.r_max;
            m_total = other.m_total;
            initial_radius = other.initial_radius;
            v_collapse = other.v_collapse;
            v_max = other.v_max;
            v_max_prev = other.v_max_prev;
            soc = other.soc;
            N = other.N;

            dynamicTime = other.dynamicTime;
            G = other.G;
            density = other.density;
            u_s = other.u_s;
            u_r = other.u_r;
            sigma = other.sigma;
            Y = other.Y;
            cor = other.cor;
            simTimeSeconds = other.simTimeSeconds;
            timeResolution = other.timeResolution;
            fourThirdsPiRho = other.fourThirdsPiRho;
            scaleBalls = other.scaleBalls;
            maxOverlap = other.maxOverlap;
            KEfactor = other.KEfactor;
            v_custom = other.v_custom;
            temp = other.temp;
            kConsts = other.kConsts;
            impactParameter = other.impactParameter;
            Ha = other.Ha;
            h_min = other.h_min;
            cone = other.cone;

            properties = other.properties;
            genBalls = other.genBalls;
            attempts = other.attempts;

            spaceRange = other.spaceRange;
            spaceRangeIncrement = other.spaceRangeIncrement;
            z0Rot = other.z0Rot;
            y0Rot = other.y0Rot;
            z1Rot = other.z1Rot;
            y1Rot = other.y1Rot;
            simTimeElapsed = other.simTimeElapsed;

            // start is const, no need to copy
            startProgress = other.startProgress;
            lastWrite = other.lastWrite;
            simTimeElapsed = other.simTimeElapsed;

            // h_min_physical and max_mu are const, no need to copy
            mu_scale = other.mu_scale;
            dynamicTime = other.dynamicTime;

            data_type = other.data_type;
            filetype = other.filetype;
            num_writes = other.num_writes;
        }
        return *this;
    }

};


/// @brief Facilitates the concept of a group of balls with physical properties.
class Ball_group
{
public:
    Ball_group_attributes attrs;


    vec3 mom = {0, 0, 0};
    vec3 ang_mom = {
        0,
        0,
        0};  // Can be vec3 because they only matter for writing out to file. Can process on host.

    double PE = 0, KE = 0;

    double* distances = nullptr;

    vec3* pos = nullptr;
    vec3* vel = nullptr;
    vec3* velh = nullptr;  ///< Velocity half step for integration purposes.
    vec3* acc = nullptr;
    vec3* w = nullptr;
    vec3* wh = nullptr;  ///< Angular velocity half step for integration purposes.
    vec3* aacc = nullptr;
    double* R = nullptr;    ///< Radius
    double* m = nullptr;    ///< Mass
    double* moi = nullptr;  ///< Moment of inertia
    // double* u_scale = nullptr; ///ADD TO COPY CONSTRUCTOR, ETC

    std::vector<double> energyBuffer;
    std::vector<double> ballBuffer;

    DECCOData* data = nullptr;
    
    Ball_group() = default;
    explicit Ball_group(const int nBalls);
    // explicit Ball_group(const std::string& path, const std::string& filename, int start_file_index);
    explicit Ball_group(std::string& path);
    explicit Ball_group(const std::string& path,const std::string& projectileName,const std::string& targetName,const double& customVel);
    Ball_group(const Ball_group& rhs);
    Ball_group& operator=(const Ball_group& rhs);
    void parse_input_file(std::string location);
    inline double calc_VDW_force_mag(const double Ra, const double Rb, const double h);
    // void calc_mu_scale_factor();
    void zeroSaveVals();
    void calibrate_dt(int const Step, const double& customSpeed);
    void pushApart() const;
    void calc_v_collapse();
    [[nodiscard]] double getVelMax();
    void calc_helpfuls();
    double get_soc();    
    void kick(const vec3& vec) const;
    vec3 calc_momentum(const std::string& of) const;
    void offset(const double& rad1, const double& rad2, const double& impactParam) const;
    [[nodiscard]] double get_radius(const vec3& center) const;
    void updateGPE();
    void sim_init_write(int counter);
    [[nodiscard]] vec3 getCOM() const;
    void zeroVel() const;
    void zeroAngVel() const;
    void to_origin() const;
    void comSpinner(const double& spinX, const double& spinY, const double& spinZ) const;
    void rotAll(const char axis, const double angle) const;
    double calc_mass(const double& radius, const double& density);
    double calc_moi(const double& radius, const double& mass);
    Ball_group spawn_particles(const int count);
    vec3 dust_agglomeration_offset(const double3x3 local_coords,vec3 projectile_pos,vec3 projectile_vel,const double projectile_rad);
    Ball_group dust_agglomeration_particle_init();
    Ball_group add_projectile();
    void merge_ball_group(const Ball_group& src);
    void freeMemory() const;
    std::string find_restart_file_name(std::string path);
    int check_restart(std::string folder);
    void loadDatafromH5(std::string path, std::string file);
    void init_data(int counter);
    std::string get_data_info();
    void parse_meta_data(std::string metadata);

    void
    sim_one_step_single_core(const bool writeStep);
    

    
private:
    // String buffers to hold data in memory until worth writing to file:
    // std::stringstream ballBuffer;
    // std::stringstream energyBuffer;


    void allocate_group(const int nBalls);
    void init_conditions();
    [[nodiscard]] double getRmin();
    [[nodiscard]] double getRmax();
    [[nodiscard]] double getMassMax() const;
    void parseSimData(std::string line);
    void loadConsts(const std::string& path, const std::string& filename);
    [[nodiscard]] static std::string getLastLine(const std::string& path, const std::string& filename);
    // void simDataWrite(std::string& outFilename);
    [[nodiscard]] double getMass();
    void threeSizeSphere(const int nBalls);
    void generate_ball_field(const int nBalls);
    void loadSim(const std::string& path, const std::string& filename);
    void distSizeSphere(const int nBalls);
    void oneSizeSphere(const int nBalls);
    void placeBalls(const int nBalls);
    void updateDTK(const double& velocity);
    void simInit_cond_and_center(bool add_prefix);
    void sim_continue(const std::string& path);
    void sim_init_two_cluster(const std::string& path,const std::string& projectileName,const std::string& targetName);
};

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

/// @brief For generating a new ballGroup 
/// @param path is a path to the job folder
Ball_group::Ball_group(std::string& path)
{
    parse_input_file(path);
    int restart = check_restart(path);
    std::string filename = find_restart_file_name(path); 
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
        loadSim(path, filename);
        calc_v_collapse(); 
        if (attrs.dt < 0)
            calibrate_dt(0, attrs.v_custom);
        simInit_cond_and_center(false);
    }
    else if (restart == 0 || just_restart)
    {

        generate_ball_field(attrs.genBalls);
        // Hack - Override and creation just 2 balls position and velocity.
        pos[0] = {0, 1.101e-5, 0};
        vel[0] = {0, 0, 0};
        if (attrs.genBalls > 1)
        {
            pos[1] = {0, -1.101e-5, 0};
            vel[1] = {0, 0, 0};
        }

        // if (attrs.mu_scale)
        // {
        //     calc_mu_scale_factor();
        // }
        // std::cerr<<initial_radius<<std::endl;

        attrs.m_total = getMass();
        calc_v_collapse();
        // std::cerr<<"INIT VCUSTOM "<<v_custom<<std::endl;
        calibrate_dt(0, attrs.v_custom);
        simInit_cond_and_center(true);
        
        
    }
    else
    {
        std::cerr<<"Simulation already complete. Now exiting. . .\n";
        exit(0);
    }
}

// /// @brief For continuing a sim.
// /// @param fullpath is the filename and path excluding the suffix _simData.csv, _constants.csv, etc.
// /// @param customVel To condition for specific vMax.
// Ball_group::Ball_group(const std::string& path, const std::string& filename, int start_file_index=0)
// {
//     parse_input_file(std::string(path));
//     sim_continue(path, filename,start_file_index);
//     calc_v_collapse();
//     calibrate_dt(0, v_custom);
//     simInit_cond_and_center(false);
// }

/// @brief For two cluster sim.
/// @param projectileName
/// @param targetName
/// @param customVel To condition for specific vMax.
Ball_group::Ball_group(
    const std::string& path,
    const std::string& projectileName,
    const std::string& targetName,
    const double& customVel=-1.)
{
    parse_input_file(std::string(path));
    // std::cerr<<path<<std::endl;
    sim_init_two_cluster(path, projectileName, targetName);
    calc_v_collapse();
    if (customVel > 0){calibrate_dt(0, customVel);}
    else {calibrate_dt(0, attrs.v_custom);}
    simInit_cond_and_center(true);
}

Ball_group& Ball_group::operator=(const Ball_group& rhs)
{
    attrs = rhs.attrs;
    // num_particles = rhs.num_particles;
    // num_particles_added = rhs.num_particles_added;

    // Useful values:
    // r_min = rhs.r_min;
    // r_max = rhs.r_max;
    // m_total = rhs.m_total;
    // initial_radius = rhs.initial_radius;
    // v_collapse = rhs.v_collapse;
    // v_max = rhs.v_max;
    // v_max_prev = rhs.v_max_prev;
    // soc = rhs.soc;

    mom = rhs.mom;
    ang_mom = rhs.ang_mom;  // Can be vec3 because they only matter for writing out to file. Can process
                            // on host.

    PE = rhs.PE;
    KE = rhs.KE;

    // skip = rhs.skip;
    // steps = rhs.steps;

    // dt=rhs.dt;
    // kin=rhs.kin;  // Spring constant
    // kout=rhs.kout;

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

    // radiiDistribution = rhs.radiiDistribution;
    // radiiFraction = rhs.radiiFraction;

    // project_path=rhs.project_path;
    // data_directory = rhs.data_directory;
    // output_folder=rhs.output_folder;
    // projectileName=rhs.projectileName;
    // targetName=rhs.targetName;
    // output_prefix=rhs.output_prefix;

    data = rhs.data;

   

    return *this;
}

Ball_group::Ball_group(const Ball_group& rhs)
{
    attrs = rhs.attrs;
    // num_particles = rhs.num_particles;
    // num_particles_added = rhs.num_particles_added;

    // Useful values:
    // r_min = rhs.r_min;
    // r_max = rhs.r_max;
    // m_total = rhs.m_total;
    // initial_radius = rhs.initial_radius;
    // v_collapse = rhs.v_collapse;
    // v_max = rhs.v_max;
    // v_max_prev = rhs.v_max_prev;
    // soc = rhs.soc;

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

    // radiiDistribution = rhs.radiiDistribution;
    // radiiFraction = rhs.radiiFraction;

    // project_path=rhs.project_path;
    // data_directory = rhs.data_directory;
    // output_folder=rhs.output_folder;
    // projectileName=rhs.projectileName;
    // targetName=rhs.targetName;
    // output_prefix=rhs.output_prefix;

    data = rhs.data;

    // skip = rhs.skip;
    // steps = rhs.steps;

    // dt=rhs.dt;
    // kin=rhs.kin;  // Spring constant
    // kout=rhs.kout;

   
}

void Ball_group::init_data(int counter = 0)
{
    // std::ifstream checkForFile;
    
    // checkForFile.open(output_folder + std::to_string(counter) + "_data."+filetype, std::ifstream::in);
    // // Add a counter to the file name until it isn't overwriting anything:
    // while (checkForFile.is_open()) {
    //     counter++;
    //     checkForFile.close();
    //     checkForFile.open(output_folder + std::to_string(counter) + '_' + "data." + filetype, std::ifstream::in);
    // }

    if (data != nullptr)
    {
        delete data;
        data = nullptr; 
    }

    std::string sav_file;
    if (attrs.data_type == 0) //h5
    {
        sav_file = attrs.output_folder+std::to_string(counter)+"_data."+attrs.filetype;
    }
    else if (attrs.data_type == 1) //csv
    {
        sav_file = attrs.output_folder+std::to_string(counter)+"_.csv";
    }
    else
    {
        std::cerr<<"ERROR: data_type '"<<attrs.filetype<<"' not supported."<<std::endl;
        exit(EXIT_FAILURE);
    }
    data = new DECCOData(sav_file,\
                        attrs.num_particles,attrs.steps/attrs.skip+1,attrs.steps);
    
}


//Parses input.json file that is in the same folder the executable is in
void Ball_group::parse_input_file(std::string location)
{

    //////TODO
    //////IF location == null then get current directory
    if (location == "")
    {
        try {
            std::filesystem::path currentPath = std::filesystem::current_path();
            location = currentPath.string() + "/";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error getting current directory: " << e.what() << std::endl;
            exit(-1);
        }
    }
    // std::string s_location(location);
    std::string json_file = location + "input.json";
    std::ifstream ifs(json_file);
    json inputs = json::parse(ifs);
    attrs.output_folder = inputs["output_folder"];
    attrs.data_directory = inputs["data_directory"];

    if (inputs["dataFormat"] == "h5" || inputs["dataFormat"] == "hdf5")
    {
        attrs.data_type = 0;
        attrs.filetype = "h5";
    }
    else if (inputs["dataFormat"] == "csv")
    {
        attrs.data_type = 1;
        attrs.filetype = "csv";
    }


    if (inputs["seed"] == std::string("default"))
    {
        attrs.seed = static_cast<unsigned int>(time(nullptr));
    }
    else
    {
        attrs.seed = static_cast<unsigned int>(inputs["seed"]);
    }
    std::ofstream seedFile;
    seedFile.open(attrs.output_folder+"seedFile.txt",std::ios::app);
    std::cerr<<"Writing seed '"<<attrs.seed<<"' to seedFile.txt"<<std::endl;
    seedFile<<attrs.seed<<std::endl;
    seedFile.close();
    random_generator.seed(attrs.seed);//This was in the else but it should be outside so random_generator is always seeded the same as srand (right?)
    srand(attrs.seed);

    std::string temp_distribution = inputs["radiiDistribution"];
    if (temp_distribution == "logNormal")
    {
        attrs.radiiDistribution = attrs.logNorm;
    }
    else
    {
        attrs.radiiDistribution = attrs.constant;
    }
    attrs.N = inputs["N"];
    attrs.dynamicTime = inputs["dynamicTime"];
    attrs.G = inputs["G"];
    attrs.density = inputs["density"];
    attrs.u_s = inputs["u_s"];
    attrs.u_r = inputs["u_r"];
    attrs.sigma = inputs["sigma"];
    attrs.Y = inputs["Y"];
    attrs.cor = inputs["cor"];
    attrs.simTimeSeconds = inputs["simTimeSeconds"];
    attrs.timeResolution = inputs["timeResolution"];
    attrs.fourThirdsPiRho = 4. / 3. * pi * attrs.density;
    attrs.scaleBalls = inputs["scaleBalls"];
    attrs.maxOverlap = inputs["maxOverlap"];
    attrs.KEfactor = inputs["KEfactor"];
    if (inputs["v_custom"] == std::string("default"))
    {
        attrs.v_custom = 0.36301555459799423;
    }
    else
    {
        attrs.v_custom = inputs["v_custom"];
    }
    attrs.temp = inputs["temp"]; // this will modify v_custom in oneSizeSphere
    double temp_kConst = inputs["kConsts"];
    attrs.kConsts = temp_kConst * (attrs.fourThirdsPiRho / (attrs.maxOverlap * attrs.maxOverlap));
    attrs.impactParameter = inputs["impactParameter"];
    attrs.Ha = inputs["Ha"];
    double temp_h_min = inputs["h_min"];
    attrs.h_min = temp_h_min * attrs.scaleBalls;
    if (inputs["cone"] == std::string("default"))
    {
        attrs.cone = pi/2;
    }
    else
    {
        attrs.cone = inputs["cone"];
    }
    attrs.properties = inputs["properties"];
    attrs.genBalls = inputs["genBalls"];
    attrs.attempts = inputs["attempts"];
    attrs.skip = inputs["skip"];
    attrs.steps = inputs["steps"];
    attrs.dt = inputs["dt"];
    attrs.kin = inputs["kin"];
    attrs.kout = inputs["kout"];
    if (inputs["spaceRange"] == std::string("default"))
    {
        attrs.spaceRange = 4 * std::pow(
                        (1. / .74 * attrs.scaleBalls * attrs.scaleBalls * attrs.scaleBalls * attrs.genBalls),
                        1. / 3.); 
    }
    else
    {
        attrs.spaceRange = inputs["spaceRange"];
    }
    if (inputs["spaceRangeIncrement"] == std::string("default"))
    {
        attrs.spaceRangeIncrement = attrs.scaleBalls * 3;
    }
    else
    {
        attrs.spaceRangeIncrement = inputs["spaceRangeIncrement"];
    }
    attrs.z0Rot = inputs["z0Rot"];
    attrs.y0Rot = inputs["y0Rot"];
    attrs.z1Rot = inputs["z1Rot"];
    attrs.y1Rot = inputs["y1Rot"];
    attrs.simTimeElapsed = inputs["simTimeElapsed"];

    attrs.projectileName = inputs["projectileName"];
    attrs.targetName = inputs["targetName"];
    attrs.output_prefix = inputs["output_prefix"];
    if (attrs.output_prefix == std::string("default"))
    {
        attrs.output_prefix = "";
    }

    attrs.radiiFraction = inputs["radiiFraction"];

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
//     for (int A = 1; A < num_particles; ++A)
//     {
//         for (int B = 0; B < A; ++B)
//         {   
//             e = static_cast<unsigned>(A * (A - 1) * .5) + B;  // a^2-a is always even, so this works.
//             u_scale[e] = calc_VDW_force_mag(R[A],R[B],h_min_physical)/
//                                 calc_VDW_force_mag(R[A],R[B],h_min);  
//         }
//     }
// }


void Ball_group::calibrate_dt(int const Step, const double& customSpeed = -1.)
{
    const double dtOld = attrs.dt;

    if (customSpeed > 0.) {
        updateDTK(customSpeed);
        std::cerr << "CUSTOM SPEED: " << customSpeed;
    } else {
        // std::cerr << vCollapse << " <- vCollapse | Lazz Calc -> " << M_PI * M_PI * G * pow(density, 4.
        // / 3.) * pow(mTotal, 2. / 3.) * rMax;

        attrs.v_max = getVelMax();

        std::cerr << '\n';

        // Take whichever velocity is greatest:
        std::cerr << attrs.v_collapse << " = vCollapse | vMax = " << attrs.v_max;
        if (attrs.v_max < attrs.v_collapse) { attrs.v_max = attrs.v_collapse; }

        if (attrs.v_max < attrs.v_max_prev) {
            updateDTK(attrs.v_max);
            attrs.v_max_prev = attrs.v_max;
            std::cerr << "\nk: " << attrs.kin << "\tdt: " << attrs.dt;
        }
    }

    if (Step == 0 or dtOld < 0) {
        attrs.steps = static_cast<int>(attrs.simTimeSeconds / attrs.dt) + 1;
        // std::cout<<simTimeSeconds / dt - steps*1.0<<std::endl;
        // if (simTimeSeconds / dt == steps) //There is one too few writes in the sim if this is true
        // {
        //     std::cout<<"IT HAPPENED, numparts: "<<num_particles<<std::endl;
        //     steps += 1;
        // }
        if (attrs.steps < 0)
        {
            std::cerr<< "ERROR: STEPS IS NEGATIVE."<<std::endl;
            std::cerr<< "simTimeSeconds/dt = " << attrs.simTimeSeconds / attrs.dt<<std::endl;
            std::cerr<< "casted simTimeSeconds/dt (steps) = " << static_cast<int>(attrs.simTimeSeconds / attrs.dt)<<std::endl;
            std::cerr<< "Exiting program now."<<std::endl;
            exit(-1);
        }

        std::cerr << "\tInitial Steps: " << attrs.steps << '\n';
    } else {
        attrs.steps = static_cast<int>(dtOld / attrs.dt) * (attrs.steps - Step) + Step;
        if (attrs.steps < 0)
        {
            std::cerr<< "ERROR: STEPS IS NEGATIVE."<<std::endl;
            std::cerr<< "dtOld/dt = " << dtOld / attrs.dt<<std::endl;
            std::cerr<< "(steps - Step) + Step = " << (attrs.steps - Step) + Step<<std::endl;
            std::cerr<< "Final steps = " << static_cast<int>(dtOld / attrs.dt) * (attrs.steps - Step) + Step<<std::endl;
            std::cerr<< "Exiting program now."<<std::endl;
            exit(-1);
        }
        std::cerr << "\tSteps: " << attrs.steps;
    }


    if (attrs.timeResolution / attrs.dt > 1.) {
        attrs.skip = static_cast<int>(floor(attrs.timeResolution / attrs.dt));
        std::cerr << "\tSkip: " << attrs.skip << '\n';
    } else {
        std::cerr << "Desired time resolution is lower than dt. Setting to 1 second per skip.\n";
        attrs.skip = static_cast<int>(floor(1. / attrs.dt));
    }
}

// todo - make bigger balls favor the middle, or, smaller balls favor the outside.
/// @brief Push balls apart until no overlaps
void Ball_group::pushApart() const
{
    std::cerr << "Separating spheres - Current max overlap:\n";
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
            std::cerr << overlapMax << "                        \r";
        } else {
            std::cerr << "\nSuccess!\n";
            break;
        }
        overlapMax = -1;
        step++;
    }
    delete[] counter;
}

void Ball_group::calc_v_collapse()
{
    // Sim fall velocity onto cluster:
    // vCollapse shrinks if a ball escapes but velMax should take over at that point, unless it is
    // ignoring far balls.
    double position = 0;
    while (position < attrs.initial_radius) {
        // todo - include vdw!!!
        attrs.v_collapse += attrs.G * attrs.m_total / (attrs.initial_radius * attrs.initial_radius) * 0.1;
        position += attrs.v_collapse * 0.1;
    }
    attrs.v_collapse = fabs(attrs.v_collapse);
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
        std::cerr << '(' << counter << " spheres ignored"
                  << ") ";
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
            std::cerr << "\nMax velocity in system is less than 1e-10.\n";
            system("pause");
        }
    }

    return attrs.v_max;
}

double Ball_group::get_soc()
{
    return attrs.soc;
}

void Ball_group::calc_helpfuls()
{
    attrs.r_min = getRmin();
    attrs.r_max = getRmax();
    attrs.m_total = getMass();
    attrs.initial_radius = get_radius(getCOM());
    attrs.soc = 4 * attrs.r_max + attrs.initial_radius;
    // soc = -1;
}   

// Kick ballGroup (give the whole thing a velocity)
void Ball_group::kick(const vec3& vec) const
{
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { vel[Ball] += vec; }
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
[[nodiscard]] double Ball_group::get_radius(const vec3& center) const
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
void Ball_group::updateGPE()
{
    PE = 0;

    if (attrs.num_particles > 1)  // Code below only necessary for effects between balls.
    {
        for (int A = 1; A < attrs.num_particles; A++) {
            for (int B = 0; B < A; B++) {
                const double sumRaRb = R[A] + R[B];
                const double dist = (pos[A] - pos[B]).norm();
                const double overlap = sumRaRb - dist;

                // Check for collision between Ball and otherBall.
                if (overlap > 0) {
                    PE +=
                        -attrs.G * m[A] * m[B] / dist + attrs.kin * ((sumRaRb - dist) * .5) * ((sumRaRb - dist) * .5);
                } else {
                    PE += -attrs.G * m[A] * m[B] / dist;
                }
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

void Ball_group::sim_init_write(int counter=0)
{
    // todo - filename is now a copy and this works. Need to consider how old way worked for
    // compatibility. What happens without setting output_prefix = filename? Check if file name already
    // exists.
    std::cerr<<"Sim init write"<<std::endl;
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

    if (attrs.data_type == 0) //This meta write is for restarting jobs. Only necessary for hdf5
    {
        data->WriteMeta(get_data_info(),attrs.sim_meta_data_name,"constants");
    }
    data->Write(constData,"constants");


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

    for (int Ball = 0; Ball < attrs.num_particles; Ball++) { pos[Ball] -= com; }
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
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
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
vec3 Ball_group::dust_agglomeration_offset(
    const double3x3 local_coords,
    vec3 projectile_pos,
    vec3 projectile_vel,
    const double projectile_rad)
{
    const auto cluster_radius = get_radius(vec3(0, 0, 0));
    bool intersect = false;
    int count = 0;
    vec3 new_position = vec3(0,0,0);
    do {
        const auto rand_y = rand_between(-cluster_radius, cluster_radius);
        const auto rand_z = rand_between(-cluster_radius, cluster_radius);
        auto test_pos = projectile_pos + perpendicular_shift(local_coords, rand_y, rand_z);

        count++;
        for (size_t i = 0; i < attrs.num_particles; i++) {
            // Check that velocity intersects one of the spheres:
            if (line_sphere_intersect(test_pos, projectile_vel, pos[i], R[i] + projectile_rad)) {
                new_position = test_pos;
                intersect = true;
                break;
            }
        }
    } while (!intersect);
    return new_position;
}

// @brief returns new ball group consisting of one particle
//        where particle is given initial conditions
//        including an random offset linearly dependant on radius 
Ball_group Ball_group::dust_agglomeration_particle_init()
{
    // Random particle to origin
    Ball_group projectile(1);
    // projectile.radiiDistribution = radiiDistribution;
    // projectile.radiiFraction = radiiFraction;
    // // projectile.data = data;
    // //carry over folders
    // projectile.project_path = project_path;
    // projectile.output_folder = output_folder;
    // projectile.data_directory = data_directory;
    // projectile.projectileName = projectileName;
    // projectile.targetName = targetName;
    // projectile.output_prefix = output_prefix;

    // projectile.skip = skip;
    // projectile.steps = steps;

    // projectile.dt=dt;
    // projectile.kin=kin;  // Spring constant
    // projectile.kout=kout;
    // Particle random position at twice radius of target:
    // We want the farthest from origin since we are offsetting form origin. Not com.
    const auto cluster_radius = get_radius(vec3(0, 0, 0));

    const vec3 projectile_direction = rand_unit_vec3();
    projectile.pos[0] = projectile_direction * (cluster_radius + attrs.scaleBalls * 4);
    if (attrs.radiiDistribution == attrs.constant)
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
    // Velocity toward origin:
    if (attrs.temp > 0)
    {
        double a = std::sqrt(Kb*attrs.temp/projectile.m[0]);
        attrs.v_custom = max_bolt_dist(a); 
        std::cerr<<"v_custom set to "<<attrs.v_custom<< "cm/s based on a temp of "
                <<attrs.temp<<" degrees K."<<std::endl; 
    }
    projectile.vel[0] = -attrs.v_custom * projectile_direction;

    
    // projectile.R[0] = 1e-5;  // rand_between(1,3)*1e-5;
    projectile.moi[0] = calc_moi(projectile.R[0], projectile.m[0]);



    const double3x3 local_coords = local_coordinates(to_double3(projectile_direction));
    
    projectile.pos[0] = dust_agglomeration_offset(local_coords,projectile.pos[0],projectile.vel[0],projectile.R[0]);


    
    return projectile;
}

// Uses previous O as target and adds one particle to hit it:
Ball_group Ball_group::add_projectile()
{
    // Load file data:
    std::cerr << "Add Particle\n";

    Ball_group projectile = dust_agglomeration_particle_init();
    
    // Collision velocity calculation:
    const vec3 p_target{calc_momentum("p_target")};
    const vec3 p_projectile{projectile.calc_momentum("p_particle")};
    const vec3 p_total{p_target + p_projectile};
    const double m_target{getMass()};
    const double m_projectile{projectile.getMass()};
    const double m_total{m_target + m_projectile};
    const vec3 v_com = p_total / m_total;

    // Negate total system momentum:
    projectile.kick(-v_com);
    kick(-v_com);

    fprintf(
        stderr,
        "\nTarget Velocity: %.2e\nProjectile Velocity: %.2e\n",
        vel[0].norm(),
        projectile.vel[0].norm());

    std::cerr << '\n';

    projectile.calc_momentum("Projectile");
    calc_momentum("Target");
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
   
    return new_group;
}

/// @brief Add another ballGroup into this one.
/// @param src The ballGroup to be added.
void Ball_group::merge_ball_group(const Ball_group& src)
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
    // // data = src.data;

    calc_helpfuls();
}

/// Allocate balls
void Ball_group::allocate_group(const int nBalls)
{
    attrs.num_particles = nBalls;

    try {
        distances = new double[(attrs.num_particles * attrs.num_particles / 2) - (attrs.num_particles / 2)];

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
        std::cerr << "Failed trying to allocate group. " << e.what() << '\n';
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


[[nodiscard]] double Ball_group::getMassMax() const
{
    double mMax = m[0];
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) {
        if (m[Ball] > mMax) { mMax = m[Ball]; }
    }
    return mMax;
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
        std::cerr << "Could not open constants file: " << constantsFilename << "... Exiting program."
                  << '\n';
        exit(EXIT_FAILURE);
    }
}


//This used to be [[nodiscard]] static std::string ... but wont compile outside the actual class definition
/// Get last line of previous simData by filename.
[[nodiscard]] std::string Ball_group::getLastLine(const std::string& path, const std::string& filename)
{
    std::string simDataFilepath = path + filename + "simData.csv";
    if (auto simDataStream = std::ifstream(simDataFilepath, std::ifstream::in)) {
        std::cerr << "\nParsing last line of data.\n";

        simDataStream.seekg(-1, std::ios_base::end);  // go to 
         // spot before the EOF

        bool keepLooping = true;
        while (keepLooping) {
            char ch = ' ';
            simDataStream.get(ch);  // Get current byte's data

            if (static_cast<int>(simDataStream.tellg()) <=
                1) {                     // If the data was at or before the 0th byte
                simDataStream.seekg(0);  // The first line is the last line
                keepLooping = false;     // So stop there
            } else if (ch == '\n') {     // If the data was a newline
                keepLooping = false;     // Stop at the current position.
            } else {                     // If the data was neither a newline nor at the 0 byte
                simDataStream.seekg(-2, std::ios_base::cur);  // Move to the front of that data, then to
                                                              // the front of the data before it
            }
        }
        std::string line;
        std::getline(simDataStream, line);  // Read the current line
        return line;
    } else {
        std::cerr << "Could not open simData file: " << simDataFilepath << "... Existing program."
                  << '\n';
        exit(EXIT_FAILURE);
    }
}

// void Ball_group::simDataWrite(std::string& outFilename)
// {
//     // todo - for some reason I need checkForFile instead of just using ballWrite. Need to work out why.
//     // Check if file name already exists. If not, initialize
//     std::ifstream checkForFile;
//     checkForFile.open(output_folder + outFilename + "simData.csv", std::ifstream::in);
//     if (checkForFile.is_open() == false) {
//         sim_init_write(outFilename);
//     } else {
//         ballBuffer << '\n';  // Prepares a new line for incoming data.

//         for (int Ball = 0; Ball < num_particles; Ball++) {
//             // Send positions and rotations to buffer:
//             if (Ball == 0) {
//                 ballBuffer << pos[Ball][0] << ',' << pos[Ball][1] << ',' << pos[Ball][2] << ','
//                            << w[Ball][0] << ',' << w[Ball][1] << ',' << w[Ball][2] << ','
//                            << w[Ball].norm() << ',' << vel[Ball].x << ',' << vel[Ball].y << ','
//                            << vel[Ball].z << ',' << 0;
//             } else {
//                 ballBuffer << ',' << pos[Ball][0] << ',' << pos[Ball][1] << ',' << pos[Ball][2] << ','
//                            << w[Ball][0] << ',' << w[Ball][1] << ',' << w[Ball][2] << ','
//                            << w[Ball].norm() << ',' << vel[Ball].x << ',' << vel[Ball].y << ','
//                            << vel[Ball].z << ',' << 0;
//             }
//         }

//         // Write simData to file and clear buffer.
//         std::ofstream ballWrite;
//         ballWrite.open(output_folder + outFilename + "simData.csv", std::ofstream::app);
//         ballWrite << ballBuffer.rdbuf();  // Barf buffer to file.
//         ballBuffer.str("");               // Resets the stream for that balls to blank.
//         ballWrite.close();
//     }
//     checkForFile.close();
// }


[[nodiscard]] double Ball_group::getMass()
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

    std::cerr << "Final spacerange: " << attrs.spaceRange << '\n';
    std::cerr << "m_total: " << attrs.m_total << '\n';
    std::cerr << "Initial Radius: " << get_radius(getCOM()) << '\n';
    std::cerr << "Mass: " << getMass() << '\n';
}

void Ball_group::generate_ball_field(const int nBalls)
{
    std::cerr << "CLUSTER FORMATION\n";

    allocate_group(nBalls);

    // Create new random number set.
        //This should be d
         // in parse_input_file
    // const int seedSave = static_cast<int>(time(nullptr));
    // srand(seedSave);
    if (attrs.radiiDistribution == attrs.constant)
    {
        oneSizeSphere(nBalls);
    }
    else
    {
        distSizeSphere(nBalls);
    }
    
    calc_helpfuls();
    // threeSizeSphere(nBalls);

    attrs.output_prefix = std::to_string(nBalls) + "_R" + scientific(get_radius(getCOM())) + "_v" +
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
        //decrease index by 1 so we have most recent finished sim
        _pos = file.find_first_of("_");
        size_t _lastpos = file.find_last_of("_");
        
        file_index = stoi(file.substr(0,_pos));

        file = std::to_string(file_index-1) + file.substr(_pos,_lastpos);
        attrs.start_index = file_index;//shouldnt be file_index-1 because that is just the one we read, we will write to the next index

        parseSimData(getLastLine(path, file));
        loadConsts(path, file);
    }
    else if (file.substr(file.size()-3,file.size()) == ".h5")
    {
        _pos = file.find_first_of("_");
        file_index = stoi(file.substr(0,_pos));
        
        //This needs to be here because its used in the following function
        attrs.start_index = file_index;

        loadDatafromH5(path,file);
    }
    else
    {
        std::cerr<<"ERROR: filename in loadSim is of unknown type."<<std::endl;
        exit(EXIT_FAILURE);
    }


    calc_helpfuls();

    std::cerr << "Balls: " << attrs.num_particles << '\n';
    std::cerr << "Mass: " << attrs.m_total << '\n';
    std::cerr << "Approximate radius: " << attrs.initial_radius << " cm.\n";
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

void Ball_group::loadDatafromH5(std::string path,std::string file)
{
    allocate_group(HDF5Handler::get_num_particles(path,file));
    
    //Load constants because this can be done without an initialized instance of DECCOData
    HDF5Handler::loadConsts(path,file,R,m,moi);

    //read metadata to determine steps and skip variables
    std::string meta = HDF5Handler::readMetadataFromDataset("constants",path+file,attrs.sim_meta_data_name);
    parse_meta_data(meta);

    //Now we have all info we need to initialze an instance of DECCOData.
    //However, data_written_so_far needs to be determined and set since this is a restart.
    //All this happens in the next two functions. 
    init_data(attrs.start_index);
    //writes is 0 if there is no writes so far (I don't think this should happen but if it does, more stuff needs to happen).
    //writes is >0 then that is how many writes there have been.
    //writes is -1 if there are writes and the sim is already finished. 
    int writes = data->setWrittenSoFar(path,file);
    // if (writes == 0)//This should really never happen. If it did then there is an empty h5 file
    // {
    //     std::cerr<<"not implimented"<<std::endl;
    //     exit(-1);
    // }
    if(writes > 0) //Works
    {
        //This cannot be done without an instance of DECCOData, that is why these are different than loadConsts
        data->loadSimData(path,file,pos,w,vel);

        //initiate buffers since we won't call sim_init_write on a restart
        energyBuffer = std::vector<double> (data->getWidth("energy")*bufferlines);
        ballBuffer = std::vector<double> (data->getWidth("simData")*bufferlines);
        
        std::cerr<<"mid_sim_restart"<<std::endl;
        attrs.mid_sim_restart = true;
        attrs.start_step = attrs.skip*(writes-1)+1;
        attrs.start_index++;
    }
    else if(writes == -1) //Works
    {
        data->loadSimData(path,file,pos,w,vel);
        attrs.start_index++;
    }
    else
    {
        std::cerr<<"ERROR: in setWrittenSoFar() output of value '"<<writes<<"'."<<std::endl;
        exit(EXIT_FAILURE);
    }
     
}

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

    std::cerr << "Final spacerange: " << attrs.spaceRange << '\n';
    std::cerr << "Initial Radius: " << get_radius(getCOM()) << '\n';
    std::cerr << "Mass: " << attrs.m_total << '\n';
}

void Ball_group::updateDTK(const double& velocity)
{
    calc_helpfuls();
    attrs.kin = attrs.kConsts * attrs.r_max * velocity * velocity;
    attrs.kout = attrs.cor * attrs.kin;
    const double h2 = attrs.h_min * attrs.h_min;
    const double four_R_min = 4 * attrs.r_min * attrs.h_min;
    const double vdw_force_max = attrs.Ha / 6 * 64 * attrs.r_min * attrs.r_min * attrs.r_min * attrs.r_min * attrs.r_min * attrs.r_min *
                                 ((attrs.h_min + attrs.r_min + attrs.r_min) / ((h2 + four_R_min) * (h2 + four_R_min) *
                                                             (h2 + four_R_min + 4 * attrs.r_min * attrs.r_min) *
                                                             (h2 + four_R_min + 4 * attrs.r_min * attrs.r_min)));
    // todo is it rmin*rmin or rmin*rmax
    const double elastic_force_max = attrs.kin * attrs.maxOverlap * attrs.r_min;
    const double regime = (vdw_force_max > elastic_force_max) ? vdw_force_max : elastic_force_max;
    const double regime_adjust = regime / (attrs.maxOverlap * attrs.r_min);

    // dt = .02 * sqrt((fourThirdsPiRho / regime_adjust) * r_min * r_min * r_min);
    attrs.dt = .01 * sqrt((attrs.fourThirdsPiRho / regime_adjust) * attrs.r_min * attrs.r_min * attrs.r_min); //NORMAL ONE
    // dt = .005 * sqrt((fourThirdsPiRho / regime_adjust) * r_min * r_min * r_min);
    std::cerr << "==================" << '\n';
    std::cerr << "dt set to: " << attrs.dt << '\n';
    std::cerr << "kin set to: " << attrs.kin << '\n';
    std::cerr << "kout set to: " << attrs.kout << '\n';
    std::cerr << "h_min set to: " << attrs.h_min << '\n';
    std::cerr << "Ha set to: " << attrs.Ha << '\n';
    std::cerr << "u_s set to: " << attrs.u_s << '\n';
    std::cerr << "u_r set to: " << attrs.u_r << '\n';
    if (vdw_force_max > elastic_force_max)
    {
        std::cerr << "In the vdw regime."<<std::endl;
    }
    else
    {
        std::cerr << "In the elastic regime."<<std::endl;
    }
    std::cerr << "==================" << '\n';
}


void Ball_group::simInit_cond_and_center(bool add_prefix)
{
    std::cerr << "==================" << '\n';
    std::cerr << "dt: " << attrs.dt << '\n';
    std::cerr << "k: " << attrs.kin << '\n';
    std::cerr << "Skip: " << attrs.skip << '\n';
    std::cerr << "Steps: " << attrs.steps << '\n';
    std::cerr << "==================" << '\n';

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


// void Ball_group::sim_continue(const std::string& path)
// {
//     // Load file data:
//     std::string filename = find_file_name(path);
//     loadSim(path, filename);
    
//     if (data_type == 0) //h5 data output
//     {
//         loadSim(path, filename);
//     }
//     else if (data_type == 1) //  If csv data_type is implimented, this part should go something like this
//     {
//         if (start_file_index == 0)
//         {
//             std::cerr << "Continuing Sim...\nFile: " << filename << '\n';
//         }
//         else
//         {
//             std::cerr << "Continuing Sim...\nFile: " << start_file_index << '_' << filename << '\n';
//             loadSim(path, std::to_string(start_file_index) + "_" + filename);
//         }
//     }
//     else
//     {
//         std::cerr<<"ERROR: ouput data type not supported yet."<<std::endl;
//     }

    



//     std::cerr << '\n';
//     calc_momentum("O");

//     // Name the file based on info above:
//     output_prefix = filename;
// }

// void Ball_group::sim_continue(const std::string& path, const std::string& filename, int start_file_index=0)
// {
//     // Load file data:
//     if (start_file_index == 0)
//     {
//         std::cerr << "Continuing Sim...\nFile: " << filename << '\n';
//         loadSim(path, filename);
//     }
//     else
//     {
//         std::cerr << "Continuing Sim...\nFile: " << start_file_index << '_' << filename << '\n';
//         loadSim(path, std::to_string(start_file_index) + "_" + filename);
//     }



//     std::cerr << '\n';
//     calc_momentum("O");

//     // Name the file based on info above:
//     output_prefix = filename;
// }


// Set's up a two cluster collision.
void Ball_group::sim_init_two_cluster(
    const std::string& path,
    const std::string& projectileName,
    const std::string& targetName)
{
    // Load file data:
    std::cerr << "TWO CLUSTER SIM\nFile 1: " << projectileName << '\t' << "File 2: " << targetName
              << '\n';

    // DART PROBE
    // ballGroup projectile(1);
    // projectile.pos[0] = { 8814, 0, 0 };
    // projectile.w[0] = { 0, 0, 0 };
    // projectile.vel[0] = { 0, 0, 0 };
    // projectile.R[0] = 78.5;
    // projectile.m[0] = 560000;
    // projectile.moi[0] = .4 * projectile.m[0] * projectile.R[0] * projectile.R[0];


    Ball_group projectile;
    projectile.loadSim(path, projectileName);
    Ball_group target;
    target.loadSim(path, targetName);

    attrs.num_particles = projectile.attrs.num_particles + target.attrs.num_particles;
    
    std::cerr<<"Total number of particles in sim: "<<attrs.num_particles<<std::endl;

    // DO YOU WANT TO STOP EVERYTHING?
    // projectile.zeroAngVel();
    // projectile.zeroVel();
    // target.zeroAngVel();
    // target.zeroVel();


    // Calc info to determined cluster positioning and collisions velocity:
    projectile.updateGPE();
    target.updateGPE();

    projectile.offset(
        projectile.attrs.initial_radius, target.attrs.initial_radius + target.getRmax() * 2, attrs.impactParameter);

    //      const double PEsys = projectile.PE + target.PE + (-G * projectile.mTotal * target.mTotal /
    //(projectile.getCOM() - target.getCOM()).norm());

    // Collision velocity calculation:
    const double mSmall = projectile.attrs.m_total;
    const double mBig = target.attrs.m_total;
    //      const double mTot = mBig + mSmall;
    // const double vSmall = -sqrt(2 * KEfactor * fabs(PEsys) * (mBig / (mSmall * mTot))); // Negative
    // because small offsets right.
    const double vSmall = -attrs.v_custom;                // DART probe override.
    const double vBig = -(mSmall / mBig) * vSmall;  // Negative to oppose projectile.
    // const double vBig = 0; // Dymorphous override.

    if (std::isnan(vSmall) || std::isnan(vBig)) {
        std::cerr << "A VELOCITY WAS NAN!!!!!!!!!!!!!!!!!!!!!!\n\n";
        exit(EXIT_FAILURE);
    }

    projectile.kick(vec3(vSmall, 0, 0));
    target.kick(vec3(vBig, 0, 0));

    fprintf(stderr, "\nTarget Velocity: %.2e\nProjectile Velocity: %.2e\n", vBig, vSmall);

    std::cerr << '\n';
    projectile.calc_momentum("Projectile");
    target.calc_momentum("Target");

    allocate_group(projectile.attrs.num_particles + target.attrs.num_particles);

    merge_ball_group(target);
    merge_ball_group(projectile);  // projectile second so smallest ball at end and largest ball at front
                                   // for dt/k calcs.

    attrs.output_prefix = projectileName + targetName + "T" + rounder(attrs.KEfactor, 4) + "_vBig" +
                    scientific(vBig) + "_vSmall" + scientific(vSmall) + "_IP" +
                    rounder(attrs.impactParameter * 180 / 3.14159, 2) + "_rho" + rounder(attrs.density, 4);
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
        file = entry.path();
        size_t pos = file.find_last_of("/");
        file = file.erase(0,pos+1);
        
        if (file.substr(0,file.size()-4) == "timing")
        {
            return 2;
        }

        //Is the data in csv format?
        if (file.substr(file.size()-4,file.size()) == ".csv")
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
        else if (file.substr(file.size()-3,file.size()) == ".h5")
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


std::string Ball_group::find_restart_file_name(std::string path)
{
    std::string file;
    std::string largest_file_name;
    int largest_file_index = -1;
    int file_index=0;
    for (const auto & entry : fs::directory_iterator(path))
    {
        file = entry.path();
        size_t pos = file.find_last_of("/");
        file = file.erase(0,pos+1);

        //Is the data in csv format?
        if (file.substr(file.size()-4,file.size()) == ".csv")
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
                largest_file_name = file;
            }
        }
        else if (file.substr(file.size()-3,file.size()) == ".h5")
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

    return largest_file_name;
}

void Ball_group::sim_one_step_single_core(const bool writeStep)
{
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

    // std::ofstream accWrite, aaccWrite;
    // accWrite.open(output_folder+"accWrite_"+std::to_string(num_particles)+".txt",std::ios::app);
    // aaccWrite.open(output_folder+"aaccWrite_"+std::to_string(num_particles)+".txt",std::ios::app);

    /// SECOND PASS - Check for collisions, apply forces and torques:
    // t.start_event("CalcForces/loopApplicablepairs");
    for (int A = 1; A < attrs.num_particles; A++)  
    {
        /// DONT DO ANYTHING HERE. A STARTS AT 1.
        for (int B = 0; B < A; B++) {
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

                // if (!contact && A == num_particles-1)
                // {
                //     // std::cout<<"CONTACT MADE"<<std::endl;
                //     contact = true;
                //     contactBuffer<<A<<','<<simTimeElapsed<<'\n';
                // }

                double k;
                if (dist >= oldDist) {
                    k = attrs.kout;
                } else {
                    k = attrs.kin;
                }

                // Cohesion (in contact) h must always be h_min:
                // constexpr double h = h_min;
                const double h = attrs.h_min;
                const double Ra = R[A];
                const double Rb = R[B];
                const double h2 = h * h;
                // constexpr double h2 = h * h;
                const double twoRah = 2 * Ra * h;
                const double twoRbh = 2 * Rb * h;
                const vec3 vdwForceOnA = attrs.Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
                                         ((h + Ra + Rb) / ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
                                                           (h2 + twoRah + twoRbh + 4 * Ra * Rb) *
                                                           (h2 + twoRah + twoRbh + 4 * Ra * Rb))) *
                                         rVecab.normalized();
                
                
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
                // if (rel_vel_mag > 1e-20)  // Divide by zero protection.
                // if (rel_vel_mag > 1e-8)  // Divide by zero protection.
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
                        slideForceOnA = attrs.u_s * elastic_force_A_mag * (frame_A_vel_B / rel_vel_mag);
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
                            -attrs.u_r * elastic_force_A_mag * (w_diff).cross(r_a) / 
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

                if (writeStep) {
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
                        -attrs.Ha / 6 *
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
                if (h < attrs.h_min)  // If h is closer to 0 (almost touching), use hmin.
                {
                    h = attrs.h_min;
                }
                const double Ra = R[A];
                const double Rb = R[B];
                const double h2 = h * h;
                const double twoRah = 2 * Ra * h;
                const double twoRbh = 2 * Rb * h;
                const vec3 vdwForceOnA = attrs.Ha / 6 * 64 * Ra * Ra * Ra * Rb * Rb * Rb *
                                         ((h + Ra + Rb) / ((h2 + twoRah + twoRbh) * (h2 + twoRah + twoRbh) *
                                                           (h2 + twoRah + twoRbh + 4 * Ra * Rb) *
                                                           (h2 + twoRah + twoRbh + 4 * Ra * Rb))) *
                                         rVecab.normalized();
                // const vec3 vdwForceOnA = {0.0,0.0,0.0};
                
                /////////////////////////////
                totalForceOnA = vdwForceOnA + gravForceOnA;
                // totalForceOnA = vdwForceOnA;
                // totalForceOnA = gravForceOnA;
                /////////////////////////////
                if (writeStep) {
                    // PE += -G * m[A] * m[B] * grav_scale / dist; // Gravitational

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

    

    // t.end_event("CalcForces/loopApplicablepairs");



    // THIRD PASS - Calculate velocity for next step:
    // t.start_event("CalcVelocityforNextStep");
    for (int Ball = 0; Ball < attrs.num_particles; Ball++) 
    {
        // Velocity for next step:
        vel[Ball] = velh[Ball] + .5 * acc[Ball] * attrs.dt;
        w[Ball] = wh[Ball] + .5 * aacc[Ball] * attrs.dt;


        if (writeStep) 
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
    if (writeStep)
    {
        attrs.num_writes ++;
    }
    // t.end_event("CalcVelocityforNextStep");
}  // one Step end


