#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "EstimatorParser.h"
#include "action/Action.h"
#include "action/ActionChoice.h"
#include "action/CoulombAction.h"
#include "action/DoubleAction.h"
#include "base/Charges.h"
#include "base/FermionWeight.h"
#include "base/MagneticFluxCalculator.h"
#include "base/MagneticFluxWeight.h"
#include "base/ModelState.h"
#include "base/SimulationInfo.h"
#include "base/SimInfoWriter.h"
#include "base/Species.h"
#include "emarate/EMARateEstimator.h"
#include "emarate/EMARateWeight.h"
#include "estimator/AngularMomentumEstimator.h"
#include "estimator/ConductivityEstimator.h"
#include "estimator/ConductivityEstimator2D.h"
#include "estimator/ConductanceEstimator.h"
#include "estimator/DensDensEstimator.h"
#include "estimator/DensityEstimator.h"
#include "estimator/DensityCurrentEstimator.h"
#include "estimator/SpinChoiceDensityEstimator.h"
#include "estimator/DensCountEstimator.h"
#include "estimator/DiamagneticEstimator.h"
#include "estimator/DynamicPCFEstimator.h"
#include "estimator/CountCountEstimator.h"
#include "estimator/FreeEnergyEstimator.h"
#include "estimator/CoulombEnergyEstimator.h"
#include "estimator/EwaldCoulombEstimator.h"
#include "estimator/ThermoEnergyEstimator.h"
#include "estimator/VirialEnergyEstimator.h"
#include "estimator/DipoleMomentEstimator.h"
#include "estimator/BondLengthEstimator.h"
#include "estimator/FrequencyEstimator.h"
#include "estimator/PositionEstimator.h"
#include "estimator/BoxEstimator.h"
#include "estimator/PairCFEstimator.h"
#include "estimator/ZeroVarDensityEstimator.h"
#include "estimator/PermutationEstimator.h"
#include "estimator/JEstimator.h"
#include "estimator/SpinChargeEstimator.h"
#include "estimator/SpinChoicePCFEstimator.h"
#include "estimator/VIndEstimator.h"
#include "estimator/EIndEstimator.h"
#include "estimator/SKOmegaEstimator.h"
#include "estimator/WindingEstimator.h"
#include "fixednode/SHOPhase.h"
#include "spin/SpinEstimator.h"
#include "stats/MPIManager.h"
#include "stats/EstimatorManager.h"
#include "stats/SimpleScalarAccumulator.h"
#include "stats/PartitionedScalarAccumulator.h"
#include "stats/Units.h"
#include "util/Distance.h"
#include "util/PairDistance.h"
#include "util/SuperCell.h"
#include "estimator/WeightEstimator.h"

EstimatorParser::EstimatorParser(const SimulationInfo& simInfo,
    const double tau, const Action* action, const DoubleAction* doubleAction,
    ActionChoiceBase *actionChoice, MPIManager *mpi)
  : parser(simInfo.getUnits()), manager(0),
    simInfo(simInfo), tau(tau), action(action), doubleAction(doubleAction),
    actionChoice(actionChoice), mpi(mpi) {
  manager=new EstimatorManager("pimc.h5", mpi, new SimInfoWriter(simInfo));
}

EstimatorParser::~EstimatorParser() {delete manager;}

void EstimatorParser::parse(const xmlXPathContextPtr& ctxt) {
    xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"//Estimators",ctxt);
    xmlNodePtr estNode=obj->nodesetval->nodeTab[0];
    // First see if we need a FreeEnergyEstimator for ActionChoice.
    if (actionChoice) {
        manager->add(new FreeEnergyEstimator(simInfo,
                actionChoice->getModelState().getModelCount(), mpi));


        bool splitOverStates = parser.getBoolAttribute(estNode,"splitOverStates");
        if (splitOverStates) {
            manager->setIsSplitOverStates(true);
            manager->setPartitionWeight(&actionChoice->getModelState());
            manager->add(new WeightEstimator(manager->createScalarAccumulator()));
        }
    }

    bool withEMARate = parser.getBoolAttribute(estNode, "withEMARate");
    std::cout << "withEMARate = " << withEMARate << std::endl;
    if (withEMARate) {
        manager->setIsSplitOverStates(true);
        double C =  parser.getDoubleAttribute(estNode,"c");
        std::string specName = parser.getStringAttribute(estNode,"species1");
        const Species& species1(simInfo.getSpecies(specName));
        specName = parser.getStringAttribute(estNode,"species2");
        const Species& species2(simInfo.getSpecies(specName));
        EMARateWeight* partitionWeight =
                new EMARateWeight(simInfo, &species1, &species2, C);
        if (parser.getBoolAttribute(estNode, "useCoulomb")) {
            double epsilon = parser.getDoubleAttribute(estNode, "epsilon");
            if (epsilon < 1e-15) epsilon = 1.0;
            int norder = parser.getIntAttribute(estNode, "norder");
            partitionWeight->includeCoulombContribution(epsilon, norder);
        }
        manager->setPartitionWeight(partitionWeight);
        manager->add(new WeightEstimator(manager->createScalarAccumulator()));
        std::cout << "Using EMA rates" << std::endl;

    }

    double bfield = parser.getEnergyAttribute(estNode, "bfield");
    if (bfield > 1e-15) {
        int partitionCount = parser.getIntAttribute(estNode, "partitions");
        Charges* charges = new Charges(&simInfo);
        MagneticFluxCalculator* fluxCalculator = new MagneticFluxCalculator(charges);
        PartitionWeight* weight = new MagneticFluxWeight(bfield, partitionCount, fluxCalculator);
        manager->setIsSplitOverStates(true);
        manager->setPartitionWeight(weight);
        manager->add(new WeightEstimator(manager->createScalarAccumulator()));
    }

    double withExactFermions = parser.getBoolAttribute(estNode, "withExactFermions");
    if (withExactFermions) {
        PartitionWeight* weight = new FermionWeight();
        manager->setIsSplitOverStates(true);
        manager->setPartitionWeight(weight);
        manager->add(new WeightEstimator(manager->createScalarAccumulator()));
    }

  // Then parse the xml estimator list.
  obj = xmlXPathEval(BAD_CAST"//Estimators/*",ctxt);
  int nest=obj->nodesetval->nodeNr;
  for (int iest=0; iest<nest; ++iest) {
    xmlNodePtr estNode=obj->nodesetval->nodeTab[iest];
    std::string name = parser.getName(estNode);
    if (name=="ThermalEnergyEstimator") {
      std::string unitName = parser.getStringAttribute(estNode,"unit");
      double shift= parser.getEnergyAttribute(estNode,"shift");
      double perN= parser.getDoubleAttribute(estNode,"perN");
      double scale = 1.;
      if (perN>0) scale/=perN;
      if (unitName!="") scale/=simInfo.getUnits()->getEnergyScaleIn(unitName);
      manager->add(new ThermoEnergyEstimator(simInfo,action,doubleAction,
          unitName, scale, shift, manager->createScalarAccumulator()));
    }
    if (name=="SpinEstimator") {
      double gc= parser.getDoubleAttribute(estNode,"gc");
      manager->add(new SpinEstimator(simInfo,0,gc));
      manager->add(new SpinEstimator(simInfo,1,gc));
      manager->add(new SpinEstimator(simInfo,2,gc));
    }
    if (name=="VirialEnergyEstimator") {
      int nwindow= parser.getIntAttribute(estNode,"nwindow");
      if (nwindow==0) nwindow=1;
      std::string unitName= parser.getStringAttribute(estNode,"unit");
      double shift= parser.getEnergyAttribute(estNode,"shift");
      int perN= parser.getIntAttribute(estNode,"perN");
      double scale = 1.;
      if (perN>0) scale/=perN;
      if (unitName!="") scale/=simInfo.getUnits()->getEnergyScaleIn(unitName);
      manager->add(
        new VirialEnergyEstimator(simInfo,action,doubleAction,nwindow,mpi,
                                  unitName,scale,shift));
    }
    if (name=="CoulombEnergyEstimator") {
      double epsilon= parser.getDoubleAttribute(estNode,"epsilon");
      if (epsilon==0) epsilon=1;
      std::string unitName= parser.getStringAttribute(estNode,"unit");
      double shift= parser.getEnergyAttribute(estNode,"shift");
      int perN= parser.getIntAttribute(estNode,"perN");
      double scale = 1.;
      if (perN>0) scale/=perN;
      if (unitName!="") scale/=simInfo.getUnits()->getEnergyScaleIn(unitName);
      bool useEwald= parser.getBoolAttribute(estNode,"useEwald");
      if (useEwald) {
        double rcut= parser.getLengthAttribute(estNode,"rcut");
        double kcut= parser.getDoubleAttribute(estNode,"kcut");
	std::string ewaldType= parser.getStringAttribute(estNode,"ewaldType");
	if (ewaldType=="") ewaldType="optEwald";
	if (ewaldType=="opt") ewaldType="optEwald";
	if (ewaldType=="trad") ewaldType="tradEwald";
	
	if (ewaldType=="" || ewaldType=="optEwald"){
	  manager->add(new EwaldCoulombEstimator(simInfo,action,
						 epsilon,rcut,kcut,unitName,scale,shift,
						 manager->createScalarAccumulator()));
	} else 
	  if ( ewaldType=="tradEwald"){
	    int nimages= parser.getIntAttribute(estNode, "ewaldImages");
	    if (nimages ==0) nimages=1;
	    double kappa= parser.getDoubleAttribute(estNode,"kappa");
	    if (kappa==0) {
	      double Lside=1000000000000000.0 ;
	      for (int i=0; i<NDIM; i++) {
		if (Lside > (*simInfo.getSuperCell()).a[i] )
		  Lside = (*simInfo.getSuperCell()).a[i];
	      }
	      kappa = sqrt( pow(3.6*simInfo.getNPart(),(1.0/6.0))*sqrt(3.1415926535897931)/(Lside) );
	    }
	    std :: cout <<"Estimator Parser :: CoulombEnergyEstimator :: kappa :: "<< kappa<<std :: endl;
	    bool testEwald= parser.getBoolAttribute(estNode,"testEwald");

	    manager->add(new EwaldCoulombEstimator(simInfo,action,
						   epsilon,rcut,kcut,unitName,scale,shift, kappa, nimages,testEwald,
						   manager->createScalarAccumulator()));
	  }
      } else {
        manager->add(new CoulombEnergyEstimator(simInfo,epsilon,
                        unitName,scale,shift,manager->createScalarAccumulator()));
      }
    }
    if (name=="AngularMomentumEstimator") {
      double omega= parser.getEnergyAttribute(estNode,"omega");
      double b= parser.getEnergyAttribute(estNode,"b");
      PhaseModel* phaseModel=new SHOPhase(simInfo,simInfo.getSpecies(0),omega,
                                          simInfo.getTemperature(),b,0);
      manager->add(new AngularMomentumEstimator(simInfo,phaseModel));
    }
    if (name=="DipoleMomentEstimator") {
      std::string component= parser.getStringAttribute(estNode,"component");
      int index=2; //default use z component
      if (component=="x") index=0;
      else if (component=="y") index=1;
      manager->add(new DipoleMomentEstimator(simInfo, index));
    }
    if (name=="BondLengthEstimator") {
      std::string species1= parser.getStringAttribute(estNode,"species1");
      std::string species2= parser.getStringAttribute(estNode,"species2");
      std::string unitName= parser.getStringAttribute(estNode,"unit");
      manager->add(new BondLengthEstimator(simInfo,simInfo.getSpecies(species1)
			      ,simInfo.getSpecies(species2),unitName));
    }
    if (name=="FrequencyEstimator") {
      std::string species1= parser.getStringAttribute(estNode,"species1");
      std::string species2= parser.getStringAttribute(estNode,"species2");
      int nfreq= parser.getIntAttribute(estNode,"nfreq");
      if (nfreq==0) nfreq=simInfo.getNSlice();
      int nstride= parser.getIntAttribute(estNode,"nstride");
      if (nstride==0) nstride=1;
      manager->add(new FrequencyEstimator(simInfo,simInfo.getSpecies(species1),
	simInfo.getSpecies(species2), nfreq, nstride, mpi));
    }
    if (name=="BoxEstimator") {
      std::string component= parser.getStringAttribute(estNode,"component");
      int index=2; //default use z component
      if (component=="x") index=0;
      else if (component=="y") index=1;
      double lower= parser.getDoubleAttribute(estNode,"lower");
      double upper= parser.getDoubleAttribute(estNode,"upper");
      std::string species= parser.getStringAttribute(estNode,"species");
      manager->add(new BoxEstimator(simInfo, simInfo.getSpecies(species), 
                                    lower, upper, index));
    }
    if (name=="PositionEstimator") {
      std::string component= parser.getStringAttribute(estNode,"component");
      int index=2; //default use z component
      if (component=="x") index=0;
      else if (component=="y") index=1;
      std::string species= parser.getStringAttribute(estNode,"species");
      manager->add(new PositionEstimator(simInfo, 
                                         simInfo.getSpecies(species), index));
    }
    if (name=="ConductivityEstimator") {
      int nbin= parser.getIntAttribute(estNode,"nbin");
      if (nbin==0) nbin=1;
      int ndbin= parser.getIntAttribute(estNode,"ndbin");
      if (ndbin==0) ndbin=1;
      int nfreq= parser.getIntAttribute(estNode,"nfreq");
      if (nfreq==0) nfreq=simInfo.getNSlice();
      int nstride= parser.getIntAttribute(estNode,"nstride");
      if (nstride==0) nstride=1;
      manager->add(
        new ConductivityEstimator(simInfo,nfreq,nbin,ndbin,nstride,mpi));
      if ( parser.getBoolAttribute(estNode,"calcInduced")) {
        std::cout << "Also calculating induced voltage" << std::endl;
        const Action *coulAction =
                action->getActionPointerByType(typeid(CoulombAction));
        if (!coulAction) {
          std::cout << "ERROR, can't find CoulombAction" << std::endl;
        } else {
          //manager->add(new VIndEstimator(simInfo,
          //    dynamic_cast<const CoulombAction*>(coulAction),
          //    nfreq,nbin,ndbin,nstride,mpi));
          manager->add(new VIndEstimator(simInfo,
              dynamic_cast<const CoulombAction*>(coulAction),
              nfreq,nbin,nstride,mpi));
        }
      }
      if ( parser.getBoolAttribute(estNode,"calcEInduced")) {
        std::cout << "Also calculating induced efield" << std::endl;
        const Action *coulAction =
                action->getActionPointerByType(typeid(CoulombAction));
        if (!coulAction) {
          std::cout << "ERROR, can't find CoulombAction" << std::endl;
        } else {
          manager->add(new EIndEstimator(simInfo,
              dynamic_cast<const CoulombAction*>(coulAction),
              nfreq,nbin,ndbin,nstride,mpi));
        }
      }
    }
    if (name=="ConductivityEstimator2D") {
      std::string dim1 =  parser.getStringAttribute(estNode, "dim1");
      if(dim1.length()==0) dim1="xy";
      std::string dim2 =  parser.getStringAttribute(estNode, "dim2");
      if(dim2.length()==0) dim2="xy";
      int nxbin= parser.getIntAttribute(estNode,"nxbin");
      if (nxbin==0) nxbin=1;
      int nybin= parser.getIntAttribute(estNode,"nybin");
      if (nybin==0) nybin=1;
      int nxdbin= parser.getIntAttribute(estNode,"nxdbin");
      if (nxdbin==0) nxdbin=1;
      int nydbin= parser.getIntAttribute(estNode,"nydbin");
      if (nydbin==0) nydbin=1;
      double xmin= parser.getLengthAttribute(estNode,"xmin");
      double xmax= parser.getLengthAttribute(estNode,"xmax");
      double ymin= parser.getLengthAttribute(estNode,"ymin");
      double ymax= parser.getLengthAttribute(estNode,"ymax");
      int nfreq= parser.getIntAttribute(estNode,"nfreq");
      if (nfreq==0) nfreq=simInfo.getNSlice();
      int nstride= parser.getIntAttribute(estNode,"nstride");
      if (nstride==0) nstride=1;
      manager->add(new ConductivityEstimator2D(simInfo, xmin, xmax, ymin, ymax, nfreq,
                       dim1, dim2, nxbin, nybin, nxdbin, nydbin, nstride ,mpi));
    }
    if (name=="SpinChargeEstimator") {
      int nbin= parser.getIntAttribute(estNode,"nbin");
      if (nbin==0) nbin=1;
      int ndbin= parser.getIntAttribute(estNode,"ndbin");
      if (ndbin==0) ndbin=1;
      int nfreq= parser.getIntAttribute(estNode,"nfreq");
      if (nfreq==0) nfreq=simInfo.getNSlice();
      int nstride= parser.getIntAttribute(estNode,"nstride");
      if (nstride==0) nstride=1;
      std::string speciesUp= parser.getStringAttribute(estNode,"speciesUp");
      std::string speciesDown= parser.getStringAttribute(estNode,"speciesDown");
      const Species &sup(simInfo.getSpecies(speciesUp));
      const Species &sdn(simInfo.getSpecies(speciesDown));
      manager->add(new SpinChargeEstimator(
                         simInfo,sup,sdn,nfreq,nbin,ndbin,nstride,mpi));
    }
    if (name=="ConductanceEstimator") {
      int nfreq= parser.getIntAttribute(estNode,"nfreq");
      if (nfreq==0) nfreq=simInfo.getNSlice();
      std::string component= parser.getStringAttribute(estNode,"component");
      int idim=-1; // Default use all components in tensor.
      if (component=="all") idim=-1;
      else if (component=="x") idim=0;
      else if (component=="y") idim=1;
      else if (component=="z") idim=2;
      bool useCharge= parser.getBoolAttribute(estNode,"useCharge");
      bool useSpeciesTensor= parser.getBoolAttribute(estNode,"useSpeciesTensor");
      int norder= parser.getIntAttribute(estNode,"norder");
      if (norder==0) norder=1;
      manager->add(new ConductanceEstimator(simInfo,nfreq,0,
                           useSpeciesTensor,idim,useCharge,mpi,norder));
    }
    if (name=="DensityEstimator" || name=="DensCountEstimator"||
        name=="DensDensEstimator" || name=="CountCountEstimator"||
	name=="DensityCurrentEstimator" ||
	name=="SpinChoiceDensityEstimator") {
      //bool useCharge= parser.getBoolAttribute(estNode,"useCharge");
      std::string species= parser.getStringAttribute(estNode,"species");
      const Species *spec = 0;
      if (species!="" && species!="all") spec=&simInfo.getSpecies(species);
      std::string species1= parser.getStringAttribute(estNode,"species1");
      const Species *spec1 = 0;
      if (species1!="" && species1!="all") spec1=&simInfo.getSpecies(species1);
      std::string species2= parser.getStringAttribute(estNode,"species2");
      const Species *spec2 = 0;
      if (species2!="" && species2!="all") spec2=&simInfo.getSpecies(species2);
      if (spec1==0) spec1=spec;
      if (spec2==0) spec2=spec;
      std::string estName= parser.getStringAttribute(estNode,"name");
      if (estName=="") {
        if (name=="DensityEstimator") {
          estName = "rho";
        } else if (name=="DensDensEstimator") {
          estName = "nn";
	} else if (name=="DensityCurrentEstimator") {
	  estName = "nj";
        } else {
          estName = "count";
        }
        if (spec) estName += species;
      }
      DensityEstimator::DistArray dist;
      std::vector<double> min_,max_;
      std::vector<int> nbin_;
      parseDistance(estNode,ctxt,dist,min_,max_,nbin_);
      IVec nbin;
      Vec min,max;
      if (dist.size()==0) {
        nbin =  parser.getIVecAttribute(estNode,"n");
        for (int idim = 0; idim < NDIM; ++ idim) {
            if (nbin(idim)==0) nbin(idim)=1;
        }
        double a =  parser.getLengthAttribute(estNode,"a");
        min=-0.5*a*nbin;
        max=0.5*a*nbin;
        for (int i=0; i<NDIM; ++i) dist.push_back(new Cart(i));
      } else {
        for (unsigned int i=0; i<NDIM; ++i) {
          if (i<dist.size()) {
            min[i]=min_[i]; max[i]=max_[i]; nbin[i]=nbin_[i];
          } else {
            min[i]=0; max[i]=1; nbin[i]=1; dist.push_back(new Distance());
          }
        }
      }
      if (name=="DensityEstimator") {
        manager->add(new DensityEstimator(simInfo,estName,spec,
                                          min,max,nbin,dist, mpi));
      } else if (name=="SpinChoiceDensityEstimator") {
	std::string spin =  parser.getStringAttribute(estNode,"spin");
	int ispin = 0; 
	estName = "rhoeup";
	if (spin=="down") {ispin = 1; estName = "rhoedn";}
	manager->add(new SpinChoiceDensityEstimator(simInfo,estName,spec,min,
	              max,nbin,dist,ispin,actionChoice->getModelState(),mpi));
      } else if (name=="DensDensEstimator") {
        DensDensEstimator::IVecN nbinN;
        int nfreq= parser.getIntAttribute(estNode,"nfreq");
        if (nfreq==0) nfreq=simInfo.getNSlice();
        int nstride= parser.getIntAttribute(estNode,"nstride");
        if (nstride==0) nstride=1;
        for (int i=0; i<NDIM; ++i) {
          nbinN[i]=nbin[i];
          nbinN[i+NDIM]=nbin[i];
        }
        nbinN[2*NDIM]=nfreq;
        manager->add(new DensDensEstimator(simInfo,estName,spec1,spec2,
                                  min,max,nbin, nbinN,dist,nstride,mpi));
      } else if (name=="DensityCurrentEstimator") {
	DensityCurrentEstimator::IVecN nbinN;
        int nfreq= parser.getIntAttribute(estNode,"nfreq");
        if (nfreq==0) nfreq=simInfo.getNSlice();
        int nstride= parser.getIntAttribute(estNode,"nstride");
        if (nstride==0) nstride=1;
	int njbin= parser.getIntAttribute(estNode,"njbin");
	if (njbin==0) njbin=1;
	nbinN[NDIM] = njbin;
        for (int i=0; i<NDIM; ++i) nbinN[i]=nbin[i];
	nbinN[NDIM+1]=nfreq;
	manager->add(new DensityCurrentEstimator(simInfo, estName, min, max,
	                        nbin, nbinN, njbin, dist, nstride, mpi));
      } else {
        int maxCount= parser.getIntAttribute(estNode,"maxCount");
        if (maxCount==0) maxCount=1;
        if (name=="DensCountEstimator") {
          DensCountEstimator::IVecN nbinN;
          for (int i=0; i<NDIM; ++i) nbinN[i]=nbin[i];
          nbinN[NDIM]=maxCount+1;
          manager->add(new DensCountEstimator(simInfo,estName,spec,
                                              min,max,nbin,nbinN,dist,mpi));
        } else {
          CountCountEstimator::IVecN nbinN;
          for (int i=0; i<NDIM; ++i) {
            nbinN[i]=nbin[i];
            nbinN[i+NDIM]=nbin[i];
          }
          nbinN[2*NDIM]=nbinN[2*NDIM+1]=maxCount+1;
          int nfreq= parser.getIntAttribute(estNode,"nfreq");
          if (nfreq==0) nfreq=simInfo.getNSlice();
          int nstride= parser.getIntAttribute(estNode,"nstride");
          if (nstride==0) nstride=1;
          nbinN[2*NDIM+2]=nfreq;
          manager->add(new CountCountEstimator(simInfo,estName,spec,
                             min,max,nbin,nbinN,dist,nstride,mpi));
        }
      }
    }
    if (name=="PairCFEstimator") {
      ctxt->node = estNode;
      xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
      int N=obj->nodesetval->nodeNr;
      switch (N) {
        case 1: manager->add(parsePairCF<1>(estNode,obj)); break;
        case 2: manager->add(parsePairCF<2>(estNode,obj)); break;
        case 3: manager->add(parsePairCF<3>(estNode,obj)); break;
        case 4: manager->add(parsePairCF<4>(estNode,obj)); break;
        case 5: manager->add(parsePairCF<5>(estNode,obj)); break;
      }
    }
    if (name=="ZeroVarDensityEstimator") {
      
      std::string name= parser.getStringAttribute(estNode,"name");
      int nspecies =  parser.getIntAttribute(estNode,"nspecies");

      if (nspecies == 0) nspecies=2;
      Species *speciesList = new Species [nspecies];
      for (int ispec=0; ispec<nspecies; ispec++){
	std::stringstream sispec;
	sispec << "species"<<(ispec+1);
	std::string speciesName= parser.getStringAttribute(estNode,sispec.str());
	std :: cout<<"Picked species "<< speciesName <<" for Zero Variance pairCF."<<std :: endl;
	speciesList[ispec]=simInfo.getSpecies(speciesName);
      }
      
      int nbin=1; double max=1.; double min=0.;
      ctxt->node = estNode;
      xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
      int N=obj->nodesetval->nodeNr; 
      for (int idist=0; idist<N; ++idist) {
	xmlNodePtr distNode=obj->nodesetval->nodeTab[idist];
	std::string name= parser.getName(distNode);
	if (name=="Radial") {
	  min= parser.getDoubleAttribute(distNode,"min");
	  max= parser.getDoubleAttribute(distNode,"max");
	  nbin= parser.getIntAttribute(distNode,"nbin");
	}
      }
   
      // delete [] speciesList;
      manager->add(new ZeroVarDensityEstimator(simInfo,name,speciesList,nspecies,min,max,nbin,action,doubleAction,mpi));
    }

    if (name=="DynamicPCFEstimator") {
      std::string name= parser.getStringAttribute(estNode,"name");
      std::string species1= parser.getStringAttribute(estNode,"species1");
      std::string species2= parser.getStringAttribute(estNode,"species2");
      const Species &s1(simInfo.getSpecies(species1));
      const Species &s2(simInfo.getSpecies(species2));
      int nfreq= parser.getIntAttribute(estNode,"nfreq");
      if (nfreq==0) nfreq=simInfo.getNSlice();
      int nstride= parser.getIntAttribute(estNode,"nstride");
      if (nstride==0) nstride=1;
      std::vector<PairDistance*> dist;
      std::vector<double> min,max;
      std::vector<int> nbin;
      parsePairDistance(estNode,ctxt,dist,min,max,nbin);
      if (dist.size()==1) {
        manager->add(new DynamicPCFEstimator(simInfo,name,&s1,&s2,
             min[0], max[0], nbin[0], nfreq, nstride, dist[0], mpi));
      }
    }
    if (name=="SpinChoicePCFEstimator") {
      std::string name =  parser.getStringAttribute(estNode, "name");
      std::string spin =  parser.getStringAttribute(estNode, "spin");
      std::string corr =  parser.getStringAttribute(estNode, "correlation");
      bool samespin = false;
      if (corr == "same") samespin = true;
      int ispin = 0;
      if (spin == "down") ispin = 1;
      std::vector<PairDistance*> dist;
      std::vector<double> tmin, tmax;
      std::vector<int> tnbin;
      parsePairDistance(estNode, ctxt, dist, tmin, tmax, tnbin);
      switch (tnbin.size()) {
	case 1: manager->add(parseSpinPair<1>(name,tmin,tmax,tnbin,dist,
					      ispin,samespin)); break;
	case 2: manager->add(parseSpinPair<2>(name,tmin,tmax,tnbin,dist,
					      ispin,samespin)); break;
	case 3: manager->add(parseSpinPair<3>(name,tmin,tmax,tnbin,dist,
					      ispin,samespin)); break;
	case 4: manager->add(parseSpinPair<4>(name,tmin,tmax,tnbin,dist,
					      ispin,samespin)); break;
	case 5: manager->add(parseSpinPair<5>(name,tmin,tmax,tnbin,dist,
					      ispin,samespin)); break;
      }
    }
    if (name=="PermutationEstimator") {
      std::string name= parser.getStringAttribute(estNode,"name");
      std::string species1= parser.getStringAttribute(estNode,"species1");
      if (species1=="") 
        species1= parser.getStringAttribute(estNode,"species");
      const Species &s1(simInfo.getSpecies(species1));
      if (species1=="") 
        species1=s1.name;
      if (name=="") name = "perm_" + species1;
      manager->add(new PermutationEstimator(simInfo, name, s1, mpi));
    }
    if (name=="JEstimator") {
      int nBField= parser.getIntAttribute(estNode,"nBField");
      double bmax= parser.getDoubleAttribute(estNode,"bmax");
      manager->add(new JEstimator(simInfo,nBField,bmax,mpi));
    }
    if (name=="DiamagneticEstimator") {
      std::string unitName= parser.getStringAttribute(estNode,"unit");
      double perN= parser.getDoubleAttribute(estNode,"perN");
      double scale =  parser.getDoubleAttribute(estNode,"scale");
      if (perN > 0.) scale/=perN;
      if (unitName!="") scale/=simInfo.getUnits()->getLengthScaleIn(unitName,3);
      manager->add(new DiamagneticEstimator(simInfo,simInfo.getTemperature(),
                                            unitName,scale));
    }
    if (name=="WindingEstimator") {
      int nmax= parser.getIntAttribute(estNode,"nmax");
      bool isChargeCoupled =  parser.getBoolAttribute(estNode,"isChargeCoupled");
      const Species *spec = 0;
      std::string name =  parser.getStringAttribute(estNode,"name");
      std::string species =  parser.getStringAttribute(estNode,"species");

      //For species-specific winding
      species= parser.getStringAttribute(estNode,"species");
      if (species!="" && species!="all") {
        std::cout <<"Picked species "<<species<<" for winding estimator"<<std::endl;
        spec=&simInfo.getSpecies(species);
        if (name=="") name = "winding_" + species;
      }


      if (name=="") name = isChargeCoupled ? "charge_winding" : "winding";
      manager->add(new WindingEstimator(simInfo,nmax,name,isChargeCoupled,spec,mpi));
    }
    if (name=="SKOmegaEstimator") {
      IVec nbin =  parser.getIVecAttribute(estNode,"n");
      blitz::TinyVector<int,NDIM+3> nbinN;
      nbinN[0] = nbinN[1] = simInfo.getNSpecies();
      for (int i=0; i<NDIM; ++i) nbinN[i+2] = (nbin[i]==0)?1:nbin[i];
      nbinN[NDIM+2] =  parser.getIntAttribute(estNode,"nfreq");
      int nstride =  parser.getIntAttribute(estNode,"nstride");
      if (nstride==0) nstride=1;
      std::string name= parser.getStringAttribute(estNode,"name");
      if (name=="") name="skomega";
      manager->add(new SKOmegaEstimator(simInfo,name,nbin,nbinN,nstride,mpi));
    }
    if (name=="EMARateEstimator") {
      double C =  parser.getDoubleAttribute(estNode,"c");
      std::string specName = parser.getStringAttribute(estNode,"species1");
      const Species& species1(simInfo.getSpecies(specName));
      specName = parser.getStringAttribute(estNode,"species2");
      const Species& species2(simInfo.getSpecies(specName));
      EMARateEstimator* estimator =
              new EMARateEstimator(simInfo, &species1, &species2, C);
      if (parser.getBoolAttribute(estNode, "useCoulomb")) {
          double epsilon = parser.getDoubleAttribute(estNode, "epsilon");
          if (epsilon < 1e-15) epsilon = 1.0;
          int norder = parser.getIntAttribute(estNode, "norder");
          estimator->includeCoulombContribution(epsilon, norder);
      }
      manager->add(estimator);
    }
  }
  xmlXPathFreeObject(obj);
}

template<int N>
PairCFEstimator<N>* EstimatorParser::parsePairCF(xmlNodePtr estNode,
    xmlXPathObjectPtr obj) {
  std::string name= parser.getStringAttribute(estNode,"name");
  // std::string species1= parser.getStringAttribute(estNode,"species1");
  //const Species &s1(simInfo.getSpecies(species1));

  int nspecies =  parser.getIntAttribute(estNode,"nspecies");
  if (nspecies == 0) nspecies=2;
  Species *speciesList = new Species [nspecies];
  for (int ispec=0; ispec<nspecies; ispec++){
    std::stringstream sispec;
    sispec << "species"<<(ispec+1);
    std::string speciesName= parser.getStringAttribute(estNode,sispec.str());
    std :: cout<<"Picked species "<< speciesName <<" for pairCF."<<std :: endl;
    speciesList[ispec]=simInfo.getSpecies(speciesName);
    }
  
  typename PairCFEstimator<N>::VecN min(0.), max(1.);
  typename PairCFEstimator<N>::IVecN nbin(1);
  typename PairCFEstimator<N>::DistN dist(N,(PairDistance*)0);
  for (int idist=0; idist<N; ++idist) {
    xmlNodePtr distNode=obj->nodesetval->nodeTab[idist];
    std::string name= parser.getName(distNode);
    if (name=="Cartesian" || name=="Cartesian1" || name=="Cartesian2") {
      std::string dirName =  parser.getStringAttribute(distNode,"dir");
      int idir=0; if (dirName=="y") idir=1; else if (dirName=="z") idir=2;
      if (name=="Cartesian") {
        dist[idist]=new PairCart(idir);
      } else if (name=="Cartesian1") {
        dist[idist]=new PairCart1(idir);
      } else if (name=="Cartesian2") {
        dist[idist]=new PairCart2(idir);
      }
      min[idist] =  parser.getLengthAttribute(distNode,"min");
      max[idist] =  parser.getLengthAttribute(distNode,"max");
      nbin[idist] =  parser.getIntAttribute(distNode,"nbin");
    } else if (name=="Radial"||name=="Radial1"||name=="Radial2") {
      std::string dirName =  parser.getStringAttribute(distNode,"dir");
      int idir=-1;
      if (dirName=="x") idir=0;
      else if (dirName=="y") idir=1;
      else if (dirName=="z") idir=2;
      if (name=="Radial") {
        dist[idist] = new PairRadial(idir);
      } else if (name=="Radial1") {
        dist[idist] = new PairRadial1(idir);
      } else if (name=="Radial2") {
        dist[idist] = new PairRadial2(idir);
      }
      min[idist] =  parser.getLengthAttribute(distNode,"min");
      max[idist] =  parser.getLengthAttribute(distNode,"max");
      nbin[idist] =  parser.getIntAttribute(distNode,"nbin");
    } else if (name=="Angle" || name=="Angle1" || name=="Angle2") {
      int idim=0, jdim=1;
      if (NDIM==1) {
        jdim=0;
      } else if (NDIM>2) {
        std::string dirName =  parser.getStringAttribute(distNode,"dir");
        if (dirName=="x") {idim=1; jdim=2;}
        else if (dirName=="y") {idim=0; jdim=2;}
      }
      if (name=="Angle") {
        dist[idist]=new PairAngle();
      } else if (name=="PlaneAngle") {
        dist[idist]=new PairPlaneAngle(idim,jdim);
      } else if (name=="Angle1") {
        dist[idist]=new PairAngle1(idim,jdim);
      } else if (name=="Angle2") {
        dist[idist]=new PairAngle2(idim,jdim);
      }
      double minv =  parser.getDoubleAttribute(distNode,"min");
      double maxv =  parser.getDoubleAttribute(distNode,"max");
      const double PI=3.14159265358793;
      if (fabs(minv-maxv)<1e-9) {minv=-PI; maxv=+PI;}
      min[idist]=minv;
      max[idist]=maxv;
      nbin[idist]= parser.getIntAttribute(distNode,"nbin");
std::cout << name << min[idist] << " - " << max[idist] << "  " << nbin << std::endl;
    }
  }
  //delete [] speciesList;
  return new PairCFEstimator<N>(simInfo,name,speciesList,nspecies,min,max,nbin,dist,mpi);
}

void EstimatorParser::parseDistance(xmlNodePtr estNode, 
    const xmlXPathContextPtr& ctxt,
    std::vector<Distance*> &darray, std::vector<double> &min,
    std::vector<double> &max, std::vector<int>& nbin) {
  ctxt->node = estNode;
  xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
  int N=obj->nodesetval->nodeNr;
  int idir=0;
  for (int idist=0; idist<N; ++idist) {
    xmlNodePtr distNode=obj->nodesetval->nodeTab[idist];
    std::string name= parser.getName(distNode);
    if (name=="Cartesian") {
      std::string dirName =  parser.getStringAttribute(distNode,"dir");
      for (int i=0; i<NDIM; ++i) if (dirName== parser.dimName.substr(i,1)) idir=i;
      darray.push_back(new Cart(idir));
      min.push_back( parser.getLengthAttribute(distNode,"min"));
      max.push_back( parser.getLengthAttribute(distNode,"max"));
      nbin.push_back( parser.getIntAttribute(distNode,"nbin"));
      idir++;
    } else if (name=="Radial") {
      int idir=-1;
      std::string dirName =  parser.getStringAttribute(distNode,"dir");
      for (int i=0; i<NDIM; ++i) if (dirName== parser.dimName.substr(i,1)) idir=i;
      darray.push_back(new Radial(idir));
      min.push_back( parser.getLengthAttribute(distNode,"min"));
      max.push_back( parser.getLengthAttribute(distNode,"max"));
      nbin.push_back( parser.getIntAttribute(distNode,"nbin"));
    }
  }
}

void EstimatorParser::parsePairDistance(xmlNodePtr estNode, 
    const xmlXPathContextPtr& ctxt,
    std::vector<PairDistance*> &darray, std::vector<double> &min,
    std::vector<double> &max, std::vector<int>& nbin) {
  ctxt->node = estNode;
  xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
  int N=obj->nodesetval->nodeNr;
  int idir=0;
  for (int idist=0; idist<N; ++idist) {
    xmlNodePtr distNode=obj->nodesetval->nodeTab[idist];
    std::string name= parser.getName(distNode);
    if (name=="Cartesian" || name=="Cartesian1" || name=="Cartesian2") {
      std::string dirName =  parser.getStringAttribute(distNode,"dir");
      idir=0;
      for (int i=0; i<NDIM; ++i) if (dirName== parser.dimName.substr(i,1)) idir=i;
      if (name=="Cartesian") {
        darray.push_back(new PairCart(idir));
      } else if (name=="Cartesian1") {
        darray.push_back(new PairCart1(idir));
      } else {
        darray.push_back(new PairCart2(idir));
      }
      min.push_back( parser.getLengthAttribute(distNode,"min"));
      max.push_back( parser.getLengthAttribute(distNode,"max"));
      nbin.push_back( parser.getIntAttribute(distNode,"nbin"));
      idir++;
    } else if (name=="Radial"||name=="Radial1"||name=="Radial2") {
      int idir=-1;
      std::string dirName =  parser.getStringAttribute(distNode,"dir");
      for (int i=0; i<NDIM; ++i) if (dirName== parser.dimName.substr(i,1)) idir=i;
      if (name=="Radial") {
        darray.push_back(new PairRadial(idir));
      } else if (name=="Radial1") {
        darray.push_back(new PairRadial1(idir));
      } else if (name=="Radial2") {
        darray.push_back(new PairRadial2(idir));
      }
      min.push_back( parser.getLengthAttribute(distNode,"min"));
      max.push_back( parser.getLengthAttribute(distNode,"max"));
      nbin.push_back( parser.getIntAttribute(distNode,"nbin"));
    } else if (name=="Angle" || name=="Angle1" || name=="Angle2") {
      int idim=0, jdim=1;
      if (NDIM==1) {
        jdim=0;
      } else if (NDIM>2) {
        std::string dirName =  parser.getStringAttribute(distNode,"dir");
        if (dirName=="x") {idim=1; jdim=2;}
        else if (dirName=="y") {idim=0; jdim=2;}
      }
      if (name=="Angle") {
        darray.push_back(new PairAngle());
      } else if (name=="PlaneAngle") {
        darray.push_back(new PairPlaneAngle(idim,jdim));
      } else if (name=="Angle1") {
        darray.push_back(new PairAngle1(idim,jdim));
      } else if (name=="Angle2") {
        darray.push_back(new PairAngle2(idim,jdim));
      }
      double minv =  parser.getDoubleAttribute(distNode,"min");
      double maxv =  parser.getDoubleAttribute(distNode,"max");
      const double PI=3.14159265358793;
      if (fabs(minv-maxv)<1e-9) {minv=-PI; maxv=+PI;}
      min.push_back(minv);
      max.push_back(maxv);
      nbin.push_back( parser.getIntAttribute(distNode,"nbin"));
    }
  }
}

template<int N>
SpinChoicePCFEstimator<N>* EstimatorParser::parseSpinPair(
    const std::string &name, const std::vector<double> &tmin, 
    const std::vector<double> &tmax, const std::vector<int> &tnbin,
    const std::vector<PairDistance*> &dist, const int &ispin, 
    const bool &samespin) {
  typename SpinChoicePCFEstimator<N>::VecN min(0.), max(1.);
  typename SpinChoicePCFEstimator<N>::IVecN nbin(1);
  for (int i=0; i<N; ++i) {
    min[i] = tmin[i];
    max[i] = tmax[i];
    nbin[i] = tnbin[i];
  }
  return new SpinChoicePCFEstimator<N>(simInfo, name, 
	simInfo.getSpecies(0), actionChoice->getModelState(),
	min, max, nbin, dist, ispin, samespin, mpi);
}


