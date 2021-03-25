#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "initializations.hpp"
#include "vector3d.hpp"

/// @brief Facilitates the concept of a group of balls with physical properties.
/// Recommended: Use ballGroup(int nBalls) constructor to allocate all the memory needed for your ballGroup size.
struct ballGroup
{
	ballGroup() = default;

	/// @brief Constructor to allocate all the memory needed for your ballGroup size.
	/// @param nBalls Number of balls to allocate.
	ballGroup(int nBalls)
	{
		allocateGroup(nBalls);
	}

	ballGroup(std::string filename)
	{
		importDataFromFile(filename);
	}

	// String buffers to hold data in memory until worth writing to file:
	std::stringstream ballBuffer;
	std::stringstream energyBuffer;

	int cNumBalls = 0;
	int cNumBallsAdded = 0;

	vector3d
		com = { 0, 0, 0 },
		mom = { 0, 0, 0 },
		angMom = { 0, 0, 0 }; // Can be vector3d because they only matter for writing out to file. Can process on host.

	double PE = 0, KE = 0;

	double* distances = 0;

	vector3d* pos = 0;
	vector3d* vel = 0;
	vector3d* velh = 0; ///< Velocity half step for integration purposes.
	vector3d* acc = 0;
	vector3d* w = 0;
	vector3d* wh = 0; ///< Angular velocity half step for integration purposes.
	vector3d* aacc = 0;
	double* R = 0; ///< Radius
	double* m = 0; ///< Mass
	double* moi = 0; ///< Moment of inertia


	/// Allocate ball property arrays.
	inline void allocateGroup(int nBalls)
	{
		cNumBalls = nBalls;

		// Todo - Notes that this formula does not work when cNumBalls is odd.
		distances = new double[(cNumBalls * cNumBalls / 2) - (cNumBalls / 2)];

		pos = new vector3d[cNumBalls];
		vel = new vector3d[cNumBalls];
		velh = new vector3d[cNumBalls];
		acc = new vector3d[cNumBalls];
		w = new vector3d[cNumBalls];
		wh = new vector3d[cNumBalls];
		aacc = new vector3d[cNumBalls];
		R = new double[cNumBalls];
		m = new double[cNumBalls];
		moi = new double[cNumBalls];
	}


	/// @brief Add another ballGroup into this one.
	/// @param src The ballGroup to be added.
	inline void addBallGroup(const ballGroup& src)
	{
		// Copy incoming data to the end of the currently loaded data.
		memcpy(&distances[cNumBallsAdded], src.distances, sizeof(src.distances[0]) * src.cNumBalls);
		memcpy(&pos[cNumBallsAdded], src.pos, sizeof(src.pos[0]) * src.cNumBalls);
		memcpy(&vel[cNumBallsAdded], src.vel, sizeof(src.vel[0]) * src.cNumBalls);
		memcpy(&velh[cNumBallsAdded], src.velh, sizeof(src.velh[0]) * src.cNumBalls);
		memcpy(&acc[cNumBallsAdded], src.acc, sizeof(src.acc[0]) * src.cNumBalls);
		memcpy(&w[cNumBallsAdded], src.w, sizeof(src.w[0]) * src.cNumBalls);
		memcpy(&wh[cNumBallsAdded], src.wh, sizeof(src.wh[0]) * src.cNumBalls);
		memcpy(&aacc[cNumBallsAdded], src.aacc, sizeof(src.aacc[0]) * src.cNumBalls);
		memcpy(&R[cNumBallsAdded], src.R, sizeof(src.R[0]) * src.cNumBalls);
		memcpy(&m[cNumBallsAdded], src.m, sizeof(src.m[0]) * src.cNumBalls);
		memcpy(&moi[cNumBallsAdded], src.moi, sizeof(src.moi[0]) * src.cNumBalls);

		// Keep track of now loaded ball set to start next set after it:
		cNumBallsAdded += src.cNumBalls;

		// DON'T FORGET TO FREEMEMORY
	}

	/// @brief Deallocate arrays to recover memory.
	inline void freeMemory()
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

	/// Approximate the radius of the ballGroup.
	inline double getRadius()
	{
		double radius = 0;

		if (cNumBalls > 1)
		{
			for (int A = 0; A < cNumBalls; A++)
			{
				for (int B = A + 1; B < cNumBalls; B++)
				{
					// Identify two farthest balls from each other. That is diameter of cluster.
					double diameter = (pos[A] - pos[B]).norm();
					if (diameter * .5 > radius)
					{
						radius = diameter * .5;
					}
				}
			}
		}
		else
		{
			radius = R[0];
		}

		return radius;
	}

	inline double getMass()
	{
		double mTotal = 0;
		{
			for (int Ball = 0; Ball < cNumBalls; Ball++)
			{
				mTotal += m[Ball];
			}
		}
		return mTotal;
	}

	inline vector3d getCOM()
	{
		double mTotal = getMass();

		if (mTotal > 0)
		{
			vector3d comNumerator = { 0, 0, 0 };
			for (int Ball = 0; Ball < cNumBalls; Ball++)
			{
				comNumerator += m[Ball] * pos[Ball];
			}
			com = comNumerator / mTotal;
			return com;
		}
		else
		{
			std::cout << "Mass of cluster is zero..\n";
			return { NULL, NULL, NULL };
		}
	}

	inline void zeroMotion()
	{
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			w[Ball] = { 0, 0, 0 };
			vel[Ball] = { 0, 0, 0 };
		}
	}

	inline void toOrigin()
	{
		vector3d com = getCOM();

		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			pos[Ball] -= com;
		}
	}

	// Set velocity of all balls such that the cluster spins:
	inline void comSpinner(const double& spinX, const double& spinY, const double& spinZ)
	{
		vector3d comRot = { spinX, spinY, spinZ }; // Rotation axis and magnitude
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			vel[Ball] += comRot.cross(pos[Ball] - com);
			w[Ball] += comRot;
		}
	}

	inline void rotAll(const char axis, const double angle)
	{
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			pos[Ball] = pos[Ball].rot(axis, angle);
			vel[Ball] = vel[Ball].rot(axis, angle);
			w[Ball] = w[Ball].rot(axis, angle);
		}
	}



	// Initialize accelerations and energy calculations:
	inline void initConditions()
	{
		KE = 0;
		PE = 0;
		mom = { 0, 0, 0 };
		angMom = { 0, 0, 0 };
		if (cNumBalls > 1) // Code below only necessary for effects between balls.
		{
			vector3d comNumerator = { 0, 0, 0 };

			// Because A starts at 1 below:
			KE += .5 * m[0] * vel[0].dot(vel[0]) + .5 * moi[0] * w[0].dot(w[0]);
			mom += m[0] * vel[0];
			angMom += m[0] * pos[0].cross(vel[0]) + moi[0] * w[0];
			for (int A = 1; A < cNumBalls; A++)
			{
				// Warning: "A" Starts at 1 not 0.
				comNumerator += m[A] * pos[A];

				for (int B = 0; B < A; B++)
				{
					double sumRaRb = R[A] + R[B];
					double dist = (pos[A] - pos[B]).norm();
					vector3d rVecab = pos[B] - pos[A];
					vector3d rVecba = -1 * rVecab;

					// Check for collision between Ball and otherBall:
					double overlap = sumRaRb - dist;
					vector3d totalForce = { 0, 0, 0 };
					vector3d aTorque = { 0, 0, 0 };
					vector3d bTorque = { 0, 0, 0 };

					// Check for collision between Ball and otherBall.
					if (overlap > 0)
					{
						// Calculate force and torque for a:
						vector3d dVel = vel[B] - vel[A];
						vector3d relativeVelOfA = dVel - dVel.dot(rVecab) * (rVecab / (dist * dist)) - w[A].cross(R[A] / sumRaRb * rVecab) - w[B].cross(R[B] / sumRaRb * rVecab);
						vector3d elasticForceOnA = -kin * overlap * .5 * (rVecab / dist);
						vector3d frictionForceOnA = { 0,0,0 };
						if (relativeVelOfA.norm() > 1e-12) // When relative velocity is very low, dividing its vector components by its magnitude below is unstable.
						{
							frictionForceOnA = mu * elasticForceOnA.norm() * (relativeVelOfA / relativeVelOfA.norm());
						}
						aTorque = (R[A] / sumRaRb) * rVecab.cross(frictionForceOnA);

						// Calculate force and torque for b:
						dVel = vel[A] - vel[B];
						vector3d relativeVelOfB = dVel - dVel.dot(rVecba) * (rVecba / (dist * dist)) - w[B].cross(R[B] / sumRaRb * rVecba) - w[A].cross(R[A] / sumRaRb * rVecba);
						vector3d elasticForceOnB = -kin * overlap * .5 * (rVecba / dist);
						vector3d frictionForceOnB = { 0,0,0 };
						if (relativeVelOfB.norm() > 1e-12)
						{
							frictionForceOnB = mu * elasticForceOnB.norm() * (relativeVelOfB / relativeVelOfB.norm());
						}
						bTorque = (R[B] / sumRaRb) * rVecba.cross(frictionForceOnB);

						vector3d gravForceOnA = (G * m[A] * m[B] / (dist * dist)) * (rVecab / dist);
						totalForce = gravForceOnA + elasticForceOnA + frictionForceOnA;
						aacc[A] += aTorque / moi[A];
						aacc[B] += bTorque / moi[B];
						PE += -G * m[A] * m[B] / dist + kin * ((sumRaRb - dist) * .5) * ((sumRaRb - dist) * .5);
					}
					else
					{
						// No collision: Include gravity only:
						vector3d gravForceOnA = (G * m[A] * m[B] / (dist * dist)) * (rVecab / dist);
						totalForce = gravForceOnA;
						PE += -G * m[A] * m[B] / dist;
					}
					// Newton's equal and opposite forces applied to acceleration of each ball:
					acc[A] += totalForce / m[A];
					acc[B] -= totalForce / m[B];
					int e = (A * (A - 1) * .5) + B;
					distances[e] = dist;
				}
				KE += .5 * m[A] * vel[A].dot(vel[A]) + .5 * moi[A] * w[A].dot(w[A]);
				mom += m[A] * vel[A];
				angMom += m[A] * pos[A].cross(vel[A]) + moi[A] * w[A];
			}
		}
		else // For the case of just one ball:
		{
			PE = 0;
			KE = .5 * m[0] * vel[0].dot(vel[0]) + .5 * moi[0] * w[0].dot(w[0]);
			mom = m[0] * vel[0];
			angMom = m[0] * pos[0].cross(vel[0]) + moi[0] * w[0];
		}
	}

	// Update Potential Energy:
	inline void updatePE()
	{
		PE = 0;

		if (cNumBalls > 1) // Code below only necessary for effects between balls.
		{
			for (int A = 1; A < cNumBalls; A++)
			{
				for (int B = 0; B < A; B++)
				{
					double sumRaRb = R[A] + R[B];
					double dist = (pos[A] - pos[B]).norm();
					double overlap = sumRaRb - dist;

					// Check for collision between Ball and otherBall.
					if (overlap > 0)
					{
						PE += -G * m[A] * m[B] / dist + kin * ((sumRaRb - dist) * .5) * ((sumRaRb - dist) * .5);
					}
					else
					{
						PE += -G * m[A] * m[B] / dist;
					}
				}
			}
		}
		else // For the case of just one ball:
		{
			PE = 0;
		}
	}


	// Kick ballGroup (give the whole thing a velocity)
	inline void kick(const double& vx, const double& vy, const double& vz)
	{
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			vel[Ball] += {vx, vy, vz};
		}
	}


	inline void checkMomentum(const std::string& of)
	{
		vector3d pTotal = { 0,0,0 };
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			pTotal += m[Ball] * vel[Ball];
		}
		printf("%s Momentum Check: %.2e, %.2e, %.2e\n", of.c_str(), pTotal.x, pTotal.y, pTotal.z);
	}

	// offset cluster
	inline void offset(const double& rad1, const double& rad2, const double& impactParam)
	{
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			pos[Ball].x += (rad1 + rad2) * cos(impactParam);
			pos[Ball].y += (rad1 + rad2) * sin(impactParam);
		}
	}


	// get max velocity
	inline double getVelMax(bool useSoc)
	{
		double vMax = 0;

		if (useSoc)
		{
			for (int Ball = 0; Ball < cNumBalls; Ball++)
			{
				if ((pos[Ball] - com).norm() < soc && vel[Ball].norm() > vMax)
				{
					vMax = vel[Ball].norm();
				}
			}
		}
		else
		{
			for (int Ball = 0; Ball < cNumBalls; Ball++)
			{
				if (vel[Ball].norm() > vMax)
				{
					vMax = vel[Ball].norm();
				}
			}
		}

		// Is vMax for some reason unreasonably small? Don't proceed. Probably a finished sim.
		if (vMax < 1e-10)
		{
			printf("\nMax velocity in system is less than 1e-10.\n");
			system("pause");
		}

		return vMax;
	}


	inline int getRmin()
	{
		int rMin = R[0];
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			if (R[Ball] < rMin)
			{
				rMin = R[Ball];
			}
		}
		return rMin;
	}

	inline int getRmax()
	{
		int rMax = R[0];
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			if (R[Ball] > rMax)
			{
				rMax = R[Ball];
			}
		}
		return rMax;
	}


	inline int getMassMax()
	{
		int mMax = m[0];
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{
			if (m[Ball] > mMax)
			{
				mMax = m[Ball];
			}
		}
		return mMax;
	}



	/// Make ballGroup from file data.
	inline void importDataFromFile(const std::string& filename)
	{
		std::string simDataFilename = filename + "simData.csv";
		std::string constantsFilename = filename + "constants.csv";

		// Get position and angular velocity data:
		if (auto simDataStream = std::ifstream(simDataFilename, std::ifstream::in))
		{
			std::string line, lineElement;

			std::cout << "\nParsing last line of data.\n";

			simDataStream.seekg(-1, std::ios_base::end); // go to one spot before the EOF

			bool keepLooping = true;
			while (keepLooping)
			{
				char ch;
				simDataStream.get(ch); // Get current byte's data

				if ((int)simDataStream.tellg() <= 1)
				{                           // If the data was at or before the 0th byte
					simDataStream.seekg(0); // The first line is the last line
					keepLooping = false;    // So stop there
				}
				else if (ch == '\n')
				{                        // If the data was a newline
					keepLooping = false; // Stop at the current position.
				}
				else
				{                                                // If the data was neither a newline nor at the 0 byte
					simDataStream.seekg(-2, std::ios_base::cur); // Move to the front of that data, then to the front of the data before it
				}
			}


			std::getline(simDataStream, line);                                              // Read the current line
			int count = std::count(line.begin(), line.end(), ',') / properties + 1;
			allocateGroup(count); // Get number of balls in file

			std::stringstream chosenLine(line); // This is the last line of the read file, containing all data for all balls at last time step

			for (int A = 0; A < cNumBalls; A++)
			{

				for (int i = 0; i < 3; i++) // Position
				{
					std::getline(chosenLine, lineElement, ',');
					pos[A][i] = std::stod(lineElement);
					//std::cout << tclus.pos[A][i]<<',';
				}
				for (int i = 0; i < 3; i++) // Angular Velocity
				{
					std::getline(chosenLine, lineElement, ',');
					w[A][i] = std::stod(lineElement);
				}
				std::getline(chosenLine, lineElement, ','); // Angular velocity magnitude skipped
				for (int i = 0; i < 3; i++)                 // velocity
				{
					std::getline(chosenLine, lineElement, ',');
					vel[A][i] = std::stod(lineElement);
				}
				for (int i = 0; i < properties - 10; i++) // We used 10 elements. This skips the rest.
				{
					std::getline(chosenLine, lineElement, ',');
				}
			}
		}
		else
		{
			std::cerr << "Could not open simData file: " << simDataFilename << "... Existing program." << '\n';
			exit(EXIT_FAILURE);
		}

		// Get radius, mass, moi:
		if (auto ConstStream = std::ifstream(constantsFilename, std::ifstream::in))
		{
			std::string line, lineElement;
			for (int A = 0; A < cNumBalls; A++)
			{
				std::getline(ConstStream, line); // Ball line.
				std::stringstream chosenLine(line);
				std::getline(chosenLine, lineElement, ','); // Radius.
				R[A] = std::stod(lineElement);
				std::getline(chosenLine, lineElement, ','); // Mass.
				m[A] = std::stod(lineElement);
				std::getline(chosenLine, lineElement, ','); // Moment of inertia.
				moi[A] = std::stod(lineElement);
			}
		}
		else
		{
			std::cerr << "Could not open constants file: " << constantsFilename << "... Existing program." << '\n';
			exit(EXIT_FAILURE);
		}

		// Bring cluster to origin and calc its radius:
		toOrigin();

		std::cout << "Balls: " << cNumBalls << '\n';
		std::cout << "Mass: " << getMass() << '\n';
		std::cout << "Approximate radius: " << getRadius() << " cm.\n";
	}

	// Todo - make bigger balls favor the middle, or, smaller balls favor the outside.
	/// @brief Push balls apart until no overlaps
	inline void pushApart()
	{
		std::cout << "Separating spheres - Current max overlap:\n";
		/// Using vel array as storage for accumulated position change.
		int* counter = new int[cNumBalls];
		for (size_t Ball = 0; Ball < cNumBalls; Ball++)
		{
			vel[Ball] = { 0,0,0 };
			counter[Ball] = 0;
		}

		double overlapMax = -1;
		double pseudoDT = getRmin() * .1;
		int step = 0;

		while (true)
		{
			//if (step % 10 == 0)
			//{
			//	simDataWrite("pushApart_");
			//}

			for (int A = 0; A < cNumBalls; A++)
			{
				for (int B = A + 1; B < cNumBalls; B++)
				{
					// Check for Ball overlap.
					vector3d rVecab = pos[B] - pos[A];
					vector3d rVecba = -1 * rVecab;
					double dist = (rVecab).norm();
					double sumRaRb = R[A] + R[B];
					double overlap = sumRaRb - dist;

					if (overlapMax < overlap)
					{
						overlapMax = overlap;
					}

					if (overlap > 0)
					{
						vel[A] += overlap * (rVecba / dist);
						vel[B] += overlap * (rVecab / dist);
						counter[A] += 1;
						counter[B] += 1;
					}
				}
			}

			for (size_t Ball = 0; Ball < cNumBalls; Ball++)
			{
				if (counter[Ball] > 0)
				{
					pos[Ball] += vel[Ball].normalized() * pseudoDT;
					vel[Ball] = { 0,0,0 };
					counter[Ball] = 0;
				}
			}

			if (overlapMax > 0)
			{
				std::cout << overlapMax << "                        \r";
			}
			else
			{
				std::cout << "\nSuccess!\n";
				break;
			}
			overlapMax = -1;
			step++;
		}
		delete[] counter;
	}




	void simDataWrite(std::string outFilename)
	{
		// TODO for some reason I need checkForFile instead of just using ballWrite. Need to work out why.
		// Check if file name already exists. If not, initialize
		std::ifstream checkForFile;
		checkForFile.open(outFilename + "simData.csv", std::ifstream::in);
		if (checkForFile.is_open() == false)
		{
			simInitWrite(outFilename);
		}
		else
		{
			ballBuffer << '\n'; // Prepares a new line for incoming data.

			for (size_t Ball = 0; Ball < cNumBalls; Ball++)
			{
				// Send positions and rotations to buffer:
				if (Ball == 0)
				{
					ballBuffer 
						<< pos[Ball][0] << ',' 
						<< pos[Ball][1] << ',' 
						<< pos[Ball][2] << ',' 
						<< w[Ball][0] << ',' 
						<< w[Ball][1] << ',' 
						<< w[Ball][2] << ',' 
						<< w[Ball].norm() << ',' 
						<< vel[Ball].x << ',' 
						<< vel[Ball].y << ',' 
						<< vel[Ball].z << ',' 
						<< 0;
				}
				else
				{
					ballBuffer << ',' 
						<< pos[Ball][0] << ',' 
						<< pos[Ball][1] << ',' 
						<< pos[Ball][2] << ',' 
						<< w[Ball][0] << ',' 
						<< w[Ball][1] << ',' 
						<< w[Ball][2] << ',' 
						<< w[Ball].norm() << ',' 
						<< vel[Ball].x << ',' 
						<< vel[Ball].y << ',' 
						<< vel[Ball].z << ',' 
						<< 0;
				}
			}

			// Write simData to file and clear buffer.
			std::ofstream ballWrite;
			ballWrite.open(outFilename + "simData.csv", std::ofstream::app);
			ballWrite << ballBuffer.rdbuf(); // Barf buffer to file.
			ballBuffer.str("");              // Resets the stream for that balls to blank.
			ballWrite.close();
		}
		checkForFile.close();
	}

	inline void simInitWrite(const std::string& filename)
	{
		// Create string for file name identifying spin combination negative is 2, positive is 1 on each axis.
		//std::string spinCombo = "";
		//for (int i = 0; i < 3; i++)
		//{
		//	if (spins[i] < 0) { spinCombo += "2"; }
		//	else if (spins[i] > 0) { spinCombo += "1"; }
		//	else { spinCombo += "0"; }
		//}

		// Append for the three files:
		std::string simDataFilename = filename + "simData.csv";
		std::string energyFilename = filename + "energy.csv";
		std::string constantsFilename = filename + "constants.csv";

		// Check if file name already exists.
		std::ifstream checkForFile;
		checkForFile.open(simDataFilename, std::ifstream::in);
		int counter = 0;
		// Add a counter to the file name until it isn't overwriting anything:
		if (checkForFile.is_open())
		{
			while (true)
			{
				if (checkForFile.is_open())
				{
					counter++;
					checkForFile.close();
					checkForFile.open(std::to_string(counter) + '_' + simDataFilename, std::ifstream::in);
				}
				else
				{
					simDataFilename = std::to_string(counter) + '_' + simDataFilename;
					constantsFilename = std::to_string(counter) + '_' + constantsFilename;
					energyFilename = std::to_string(counter) + '_' + energyFilename;
					break;
				}
			}
		}
		checkForFile.close();

		std::cout << "New file tag: " << simDataFilename;

		// Open all file streams:
		std::ofstream energyWrite, ballWrite, constWrite;
		energyWrite.open(energyFilename, std::ofstream::app);
		ballWrite.open(simDataFilename, std::ofstream::app);
		constWrite.open(constantsFilename, std::ofstream::app);

		// Make column headers:
		energyWrite << "Time,PE,KE,E,p,L";
		ballWrite << "x0,y0,z0,wx0,wy0,wz0,wmag0,vx0,vy0,vz0,bound0";

		for (int Ball = 1; Ball < cNumBalls; Ball++) // Start at 2nd ball because first one was just written^.
		{
			std::string thisBall = std::to_string(Ball);
			ballWrite
				<< ",x" + thisBall
				<< ",y" + thisBall
				<< ",z" + thisBall
				<< ",wx" + thisBall
				<< ",wy" + thisBall
				<< ",wz" + thisBall
				<< ",wmag" + thisBall
				<< ",vx" + thisBall
				<< ",vy" + thisBall
				<< ",vz" + thisBall
				<< ",bound" + thisBall;
		}

		std::cout << "\nSim data, energy, and constants file streams and headers created.";

		// Write constant data:
		for (int Ball = 0; Ball < cNumBalls; Ball++)
		{

			constWrite
				<< R[Ball] << ','
				<< m[Ball] << ','
				<< moi[Ball]
				<< '\n';
		}

		// Write energy data to buffer:
		energyBuffer
			<< '\n'
			<< simTimeElapsed << ','
			<< PE << ','
			<< KE << ','
			<< PE + KE << ','
			<< mom.norm() << ','
			<< angMom.norm();
		energyWrite << energyBuffer.rdbuf();
		energyBuffer.str("");

		// Reinitialize energies for next step:
		KE = 0;
		PE = 0;
		mom = { 0, 0, 0 };
		angMom = { 0, 0, 0 };

		// Send position and rotation to buffer:
		ballBuffer << '\n'; // Necessary new line after header.
		ballBuffer
			<< pos[0].x << ','
			<< pos[0].y << ','
			<< pos[0].z << ','
			<< w[0].x << ','
			<< w[0].y << ','
			<< w[0].z << ','
			<< w[0].norm() << ','
			<< vel[0].x << ','
			<< vel[0].y << ','
			<< vel[0].z << ','
			<< 0; //bound[0];
		for (int Ball = 1; Ball < cNumBalls; Ball++)
		{
			ballBuffer
				<< ',' << pos[Ball].x << ',' // Needs comma start so the last bound doesn't have a dangling comma.
				<< pos[Ball].y << ','
				<< pos[Ball].z << ','
				<< w[Ball].x << ','
				<< w[Ball].y << ','
				<< w[Ball].z << ','
				<< w[Ball].norm() << ','
				<< vel[Ball].x << ','
				<< vel[Ball].y << ','
				<< vel[Ball].z << ','
				<< 0; //bound[Ball];
		}
		// Write position and rotation data to file:
		ballWrite << ballBuffer.rdbuf();
		ballBuffer.str(""); // Resets the stream buffer to blank.

		// Close Streams for user viewing:
		energyWrite.close();
		ballWrite.close();
		constWrite.close();

		std::cout << "\nInitial conditions exported and file streams closed.\nSimulating " << steps * dt / 60 / 60 << " hours.\n";
		std::cout << "Total mass: " << getMass() << '\n';
		std::cout << "\n===============================================================\n";
	}

};