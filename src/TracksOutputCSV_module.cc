/**
 * @Author: Billy Li <billyli>
 * @Date:   08-21-2021
 * @Email:  li000400@umn.edu
 * @Last modified by:   billyli
 * @Last modified time: 01-13-2022
 */



// This script extracts mcdigi and reco-hits' info to a SQL database.
// Each track is represented by a particleID
// ParticleID comply with: runID.subrunID.eventID.trackID
// where trackID is the cet::map_vector::key of SimParticle
// Currently, the script just supports StrawHit
// Further version will support PanelHit
// Author: Billy Haoyang Li
// Email: li000400@umn.edu

// C++ include
#include <ios>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <map>
#include <unistd.h>
// file system include
#include <string>
#include <boost/algorithm/string.hpp>
// Framework includes
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Sequence.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h" // For DEFINE_ART_MODULE
// Root includes
#include "TH1F.h"
#include "TTree.h"
#include "TMath.h"
// Mu2e Data Products
#include "DataProducts/inc/StrawId.hh"
#include "RecoDataProducts/inc/ComboHit.hh"
#include "MCDataProducts/inc/StrawDigiMC.hh"
#include "MCDataProducts/inc/SimParticle.hh"
// cet lib
#include "cetlib/map_vector.h"
// sqlite include
#include "sqlite3.h"

// string...
using std::to_string;

// CLHEP
using CLHEP::Hep3Vector;

namespace mu2e{
	class TracksOutputCSV : public art::EDAnalyzer {

    public:
			struct Config {
				using Name=fhicl::Name;
				using Comment=fhicl::Comment;

				fhicl::Atom<int> verbose{Name("verbose"), Comment("verbosity level (0-10)"), 0};

				fhicl::Atom<art::InputTag> mcdigisTag{ Name("StrawDigiMCCollection"), Comment("MC digi tag")};
				fhicl::Atom<art::InputTag> chTag{ Name("ComboHitCollection"), Comment("Use strawHit instead of comboHit")};
				fhicl::Atom<std::string> dataSetName{Name("dataSetName"), Comment("Nature of dataset")};
			};

			// the following line is needed to enable art --print-description
			typedef art::EDAnalyzer::Table<Config> Parameters;

    	explicit TracksOutputCSV(const Parameters&);
      void analyze(const art::Event& event) override;
			void beginJob() override;
      void endJob() override;

    private:
			/// fcl config data
			Config _conf;
    	int _verbose;

			//// Database functions
			void create_DB(std::string &dbName);
			void append_ptcl(int &ptclId, int &run, int &subrun, int &event, int &track, int &pdgId);
			void append_digi(int &digiId,int &ptclId, double &x, double &y, double &z, double &t, double &p, int &station, int &plane, int &panel, int &layer, int &straw, int &uniquePanel, int &uniqueFace, int &uniqueStraw);
			void append_hit(int &hitId, int &ptclId, int &digiId, double &x, double &y, double &z, double &t, int &station, int &plane, int &panel, int &layer, int &straw, int &uniquePanel, int &uniqueFace, int &uniqueStraw);

			// data tag
			const ComboHitCollection* _chcol;
      const StrawDigiMCCollection* _mcdigis;

			// working directory info
			std::string cwd_ideal;
			std::string cwd;
			std::string reco_dir;
			std::string mc_dir;

			// event info
			std::string dataSetName;
			int runNum;
			int subrunNum;
			int eventNum;

			// track info
			int particleID;
			int strawDigiMCID;
			int strawHitID;

			// table path
			std::string Particle_path;
			std::string StrawDigiMC_path;
			std::string StrawHit_path;

			// table writters
			std::ofstream Particle_writter;
			std::ofstream StrawDigiMC_writter;
			std::ofstream StrawHit_writter;
    };

  // Constructor
  TracksOutputCSV::TracksOutputCSV(Parameters const& conf)
	: art::EDAnalyzer(conf)
	, _conf(conf())
	, _verbose(conf().verbose())
	{}

	// begin job
	void TracksOutputCSV::beginJob(){
		dataSetName = _conf.dataSetName();
		create_DB(dataSetName);

		// initialize indices
		particleID = 0;
		strawDigiMCID = 0;
		strawHitID = 0;
	}

  // end job
  void TracksOutputCSV::endJob(){
		Particle_writter.close();
		StrawDigiMC_writter.close();
		StrawHit_writter.close();
	}

  // Analyzer
  void TracksOutputCSV::analyze(const art::Event& event){

		// Get run, subrun, and event number
		runNum = event.run();
		subrunNum = event.subRun();
		eventNum = event.event();

    // extract StrawDigiMC collections
    auto mcdH = event.getValidHandle<StrawDigiMCCollection>(_conf.mcdigisTag());
    _mcdigis = mcdH.product();

    // extract ComboHit collections
		auto chH = event.getValidHandle<ComboHitCollection>(_conf.chTag());
		_chcol = chH.product();

		// construct trackId --> particleId map
		cet::map_vector<int> trackID_ParticleID_map;

    // loop through hits
    unsigned nstrs = _mcdigis->size();
		for (unsigned istr=0; istr < nstrs; istr++){

      // Get the SimParticle Pointer from the collection
      StrawDigiMC const& mcdigi = _mcdigis->at(istr);
      StrawEnd itdc;
      art::Ptr<StepPointMC> const& spmcp = mcdigi.stepPointMC(itdc);
			cet::map_vector_key trackKey = spmcp->trackId();
			art::Ptr<SimParticle> const& spp = spmcp->simParticle();
			int spID = spp->pdgId(); // Get SimParticle's pdgID


			if (!trackID_ParticleID_map.has(trackKey)){
				// update map
				particleID ++;
				trackID_ParticleID_map[trackKey] = particleID;
				// append the particle into table
				int trackId = trackKey.asInt();
				append_ptcl(particleID, runNum, subrunNum, eventNum, trackId, spID);
			}


			int particleID = trackID_ParticleID_map[trackKey];

			// Get StrawDigiMC's x, y, z, t, p
			strawDigiMCID++;
      Hep3Vector pos_mc = spmcp->position();
      double x = pos_mc.x();
      double y = pos_mc.y();
      double z = pos_mc.z();
			double t = spmcp->time();
			double p = spmcp->momentum().mag();
			StrawId strawIdMC = spmcp->strawId();
			int stationMC = strawIdMC.getStation();
			int planeMC = strawIdMC.getPlane();
			int panelMC = strawIdMC.getPanel();
			int layerMC = strawIdMC.getLayer();
			int strawMC = strawIdMC.getStraw();
			int uniquePanelMC = strawIdMC.uniquePanel();
			int uniqueFaceMC = strawIdMC.uniqueFace();
			int uniqueStrawMC = strawIdMC.uniqueStraw();

			// append the StrawDigiMC into table
			append_digi(strawDigiMCID, particleID, x, y, z, t, p, stationMC, planeMC, panelMC, layerMC, strawMC, uniquePanelMC, uniqueFaceMC, uniqueStrawMC);

      // Get the reconstructed StrawHits information
			// including:
			// 'x, y, z
			// corrected time
			// strawId
			strawHitID++;
      ComboHit const& ch = _chcol->at(istr);
      Hep3Vector pos_reco = ch.posCLHEP();
      double x_reco = pos_reco.x();
      double y_reco = pos_reco.y();
      double z_reco = pos_reco.z();
			double t_reco = ch.correctedTime();
			StrawId strawId = ch.strawId();
			int station = strawId.getStation();
			int plane = strawId.getPlane();
			int panel = strawId.getPanel();
			int layer = strawId.getLayer();
			int straw = strawId.getStraw();
			int uniquePanel = strawId.uniquePanel();
			int uniqueFace = strawId.uniqueFace();
			int uniqueStraw = strawId.uniqueStraw();

			// append the StrawHit into table
			append_hit(strawHitID, particleID, strawDigiMCID, x_reco, y_reco, z_reco, t_reco, station, plane, panel, layer, straw, uniquePanel, uniqueFace, uniqueStraw);
		}// end of loop of hits

  }// end of analyzer

	void TracksOutputCSV::create_DB(std::string &dbName)
	{

		std::cout << "Creating csv files for " << dbName << "\n";

    // Below is the directory the script should be called
    // This absolute method should be changed if Billy want it to apply for
    // other people
    cwd_ideal = "/nashome/h/haoyang/mu2e/working/Satellite";

    // Get the current calling directory
		cwd = boost::filesystem::current_path().string();

		// Check the running directory
		if (cwd != cwd_ideal){
			std::cout << "Incorrect working directory \nShould work in: ";
			std::cout << cwd_ideal;
			std::cout << "\nNow working in: " << cwd << "\n";
			std::exit(0);
		}

    // Construct Tracking/tracks if not constructed
    std::string trkDir = cwd_ideal+"/Tracking/tracks/";
    boost::filesystem::create_directory(trkDir);

    // Check if an old database exists
    Particle_path = trkDir+dbName+"_Particle.csv";
		StrawDigiMC_path = trkDir+dbName+"_StrawDigiMC.csv";
		StrawHit_path = trkDir+dbName+"_StrawHit.csv";
    bool oldDbExist1 = boost::filesystem::exists(Particle_path);
		bool oldDbExist2 = boost::filesystem::exists(StrawDigiMC_path);
		bool oldDbExist3 = boost::filesystem::exists(StrawHit_path);

    // If an old DB exists,
    // delete all old files and csvDir before saving to DB
    (oldDbExist1)?boost::filesystem::remove(Particle_path):true;
		(oldDbExist2)?boost::filesystem::remove(StrawDigiMC_path):true;
		(oldDbExist3)?boost::filesystem::remove(StrawHit_path):true;

		// connect the db
		Particle_writter.open(Particle_path, std::ios::app);
		StrawDigiMC_writter.open(StrawDigiMC_path, std::ios::app);
		StrawHit_writter.open(StrawHit_path, std::ios::app);

		// create tables
		// Particle_writter << "id,run,subRun,event,track,pdgId" << std::endl;
		//
		// StrawDigiMC_writter << "id,particle,x,y,z,t,p,station,plane,panel,layer,"
		// 												"straw,uniquePanel,uniqueFace,uniqueStraw" << std::endl;
		//
		// StrawHit_writter << "id,particle,strawDigiMC,x_reco,y_reco,z_reco,t_reco,"
		// 										"station,plane,panel,layer,straw,"
		// 										"uniquePanel,uniqueFace,uniqueStraw" << std::endl;
	}

	// append particle
	void TracksOutputCSV::append_ptcl(int &ptclId, int &run, int &subrun, int &event, int &track, int &pdgId)
	{
		Particle_writter << ptclId << "," << run << "," << subrun << "," << event << ",";
		Particle_writter << track << "," << pdgId << std::endl;
	}

	// append StrawDigiMC
	void TracksOutputCSV::append_digi(int &digiId,int &ptclId, double &x, double &y, double &z, double &t, double &p, int &station, int &plane, int &panel, int &layer, int &straw, int &uniquePanel, int &uniqueFace, int &uniqueStraw)
	{
		StrawDigiMC_writter << digiId << "," << ptclId << ",";
		StrawDigiMC_writter << x << "," << y << "," << z << "," << t << "," << p << ",";
		StrawDigiMC_writter << station << "," << plane << "," << panel << "," << layer << "," << straw << "," << uniquePanel << "," << uniqueFace << "," << uniqueStraw << std::endl;
	}

	// append StrawHit
	void TracksOutputCSV::append_hit(int &hitId, int &ptclId, int &digiId, double &x, double &y, double &z, double &t, int &station, int &plane, int &panel, int &layer, int &straw, int &uniquePanel, int &uniqueFace, int &uniqueStraw)
	{
		StrawHit_writter << hitId << "," << ptclId << "," << digiId << ",";
		StrawHit_writter << x << "," << y << "," << z << "," << t << ",";
		StrawHit_writter << station << "," << plane << "," << panel << "," << layer << "," << straw << "," << uniquePanel << "," << uniqueFace << "," << uniqueStraw << std::endl;
	}
} // end namespace mu2e

using mu2e::TracksOutputCSV;
DEFINE_ART_MODULE(mu2e::TracksOutputCSV);
