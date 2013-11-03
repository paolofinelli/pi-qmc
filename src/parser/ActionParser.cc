#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ActionParser.h"
#include "action/Action.h"
#include "action/ActionChoice.h"
#include "action/CaoBerneAction.h"
#include "action/SphereAction.h"
#include "action/CompositeAction.h"
#include "action/CompositeDoubleAction.h"
#include "action/DoubleActionChoice.h"
#include "action/SpringTensorAction.h"
#include "action/CoulombAction.h"
#include "action/PairAction.h"
#include "action/interaction/AzizPotential.h"
#include "action/interaction/InverseCosh2Potential.h"
#include "action/interaction/LennardJonesPotential.h"
#include "action/interaction/PrimitivePairAction.h"
#include "action/interaction/PairPotential.h"
#include "action/GridPotential.h"
#include "action/HyperbolicAction.h"
#include "action/QPCAction.h"
#include "action/TimpQPC.h"
#include "action/TwoQDAction.h"
#include "action/JelliumSlab.h"
#include "action/SmoothedGridPotential.h"
#include "action/SHOAction.h"
#include "action/SHODotAction.h"
#include "action/DotGeomAction.h"
#include "action/PrimSHOAction.h"
#include "action/PrimImageAction.h"
#include "action/PrimCosineAction.h"
#include "action/PrimShellAction.h"
#include "action/PrimColloidalAction.h"
#include "action/PrimTorusAction.h"
#include "action/PrimAnisSHOAction.h"
#include "action/EFieldAction.h"
#include "action/EwaldAction.h"
#include "action/StillWebAction.h"
#include "action/RingGateAction.h"
#include "action/interaction/PairIntegrator.h"
#include "action/WellImageAction.h"
#include "action/GateAction.h"
#include "action/GaussianAction.h"
#include "action/GaussianDotAction.h"
#include "action/OpticalLatticeAction.h"
#include "action/SpringAction.h"
#include "base/Species.h"
#include "base/Beads.h"
#include "base/EnumeratedModelState.h"
#include "base/SimulationInfo.h"
#include "emarate/EMARateAction.h"
#include "fixednode/AnisotropicNodes.h"
#include "fixednode/AugmentedNodes.h"
#include "fixednode/AtomicOrbitalDM.h"
#include "fixednode/Atomic1sDM.h"
#include "fixednode/Atomic2spDM.h"
#include "fixednode/ExcitonNodes.h"
#include "fixednode/FixedNodeAction.h"
#include "fixednode/FreeParticleNodes.h"
#include "fixednode/FreePartNodesNoUpdate.h"
#include "fixednode/GroundStateSNode.h"
#include "fixednode/GroundStateWFNodes.h"
#include "fixednode/SHONodes.h"
#include "fixednode/WireNodes.h"
#include "fixednode/SpinChoiceFixedNodeAction.h"
#include "fixednode/FixedPhaseAction.h"
#include "fixednode/SHOPhase.h"
#include "fixednode/Spin4DPhase.h"
#include "spin/SpinAction.h"
#include "spin/SpinFixedPhaseAction.h"
#include "spin/LatticeSpinPhase.h"
#include "spin/SpinPhase.h"
#include "stats/MPIManager.h"
#include <iostream>
#include <cstdlib>
#include <cstdlib>
#include <blitz/tinyvec-et.h>

ActionParser::ActionParser(const SimulationInfo& simInfo, const int maxlevel,
  MPIManager *mpi)
  : XMLUnitParser(simInfo.getUnits()), 
    action(0), doubleAction(0), actionChoice(0),
    simInfo(simInfo), tau(simInfo.getTau()),
    maxlevel(maxlevel), mpi(mpi) {
}

void ActionParser::parse(const xmlXPathContextPtr& ctxt) {
  xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"//Action/*",ctxt);
  int naction=obj->nodesetval->nodeNr;
  CompositeAction* composite=new CompositeAction(naction); action=composite;
  CompositeDoubleAction* doubleComposite=new CompositeDoubleAction(naction);
  doubleAction=doubleComposite;
  parseActions(ctxt,obj,composite,doubleComposite);
  xmlXPathFreeObject(obj);
  if (doubleComposite->getCount()==0) {
     doubleAction=0; delete doubleComposite;
  }
}

void ActionParser::parseActions(const xmlXPathContextPtr& ctxt,
    xmlXPathObjectPtr& obj, CompositeAction* composite,
    CompositeDoubleAction* doubleComposite) {
  int naction=obj->nodesetval->nodeNr;
  for (int iaction=0; iaction<naction; ++iaction) {
    xmlNodePtr actNode=obj->nodesetval->nodeTab[iaction];
    std::string name=getName(actNode);
    //std::cout << "Action node name: " << name << std::endl;
    if (name=="SpringAction") {
      double pgDelta=getDoubleAttribute(actNode,"pgDelta");
      if (pgDelta==0) pgDelta=0.02;
      composite->addAction(new SpringAction(simInfo,maxlevel,pgDelta));
      continue;
    } else if (name=="HyperbolicAction") {
      composite->addAction(new HyperbolicAction(simInfo,maxlevel));
      continue;
    } else if (name=="CoulombAction") {
      double epsilon=getDoubleAttribute(actNode,"epsilon");
      if (epsilon==0) epsilon=1;
      int norder=getIntAttribute(actNode,"norder");
      double rmin=getLengthAttribute(actNode,"rmin");
      double rmax=getLengthAttribute(actNode,"rmax");
      int ngpts=getIntAttribute(actNode,"ngridPoints");
      bool dumpFiles=getBoolAttribute(actNode,"dumpFiles");
      bool useEwald=getBoolAttribute(actNode,"useEwald");
      int exLevel=getIntAttribute(actNode,"exchangeLevel");
      std::string ewaldType=getStringAttribute(actNode,"ewaldType");
      if (ewaldType=="") ewaldType="optEwald";
      if (ewaldType=="opt") ewaldType="optEwald";
      if (ewaldType=="trad") ewaldType="tradEwald";

      int ndim=getIntAttribute(actNode,"ewaldNDim");
      if (ndim==0) ndim=NDIM;
      double rcut=getLengthAttribute(actNode,"ewaldRcut");
      int nimages=getIntAttribute(actNode, "ewaldImages");
      if (nimages ==0) nimages=1;
      double kappa=getDoubleAttribute(actNode,"kappa");
      if (kappa==0) {
	double Lside=1000000000000000.0 ;
	for (int i=0; i<NDIM; i++) {
	  if (Lside > (*simInfo.getSuperCell()).a[i] )
	    Lside = (*simInfo.getSuperCell()).a[i];
	}
	kappa = sqrt( pow(3.6*simInfo.getNPart(),(1.0/6.0))*sqrt(3.1415926535897931)/Lside );
      }
      std :: cout <<"ActionParser :: CoulombAction :: kappa :: "<< kappa<<std :: endl;
      double kcut=getDoubleAttribute(actNode,"ewaldKcut");
      double screenDist=getLengthAttribute(actNode,"screenDist");
      composite->addAction(
        new CoulombAction(epsilon,simInfo,norder,rmin,rmax,ngpts,dumpFiles,
          useEwald,ndim,rcut,kcut,screenDist,kappa,nimages,ewaldType,exLevel));
      continue;
    } else if (name=="GaussianAction") {
      double v0=getEnergyAttribute(actNode,"v0");
      double alpha=getDoubleAttribute(actNode,"alpha");
      composite->addAction(new GaussianAction(v0,alpha,simInfo));
      continue;
    } else if (name=="GridPotential") {
      std::string fileName=getStringAttribute(actNode,"file");
      bool usePiezo=getBoolAttribute(actNode,"usePiezo");
      if (fileName=="") fileName="emagrids.h5";
      composite->addAction(new GridPotential(simInfo,fileName,usePiezo));
      continue;
    } else if (name=="SmoothedGridPotential") {
      std::string fileName=getStringAttribute(actNode,"file");
      if (fileName=="") fileName="emagrids.h5";
      int maxLevel=getIntAttribute(actNode,"level");
      if (maxLevel==0) maxLevel=12; //maybe this should be 8
      composite->addAction(new SmoothedGridPotential(simInfo,maxLevel,fileName));
      continue;
    } else if (name=="OpticalLatticeAction") {
      Vec v0;
      for (int idim=0; idim<NDIM; ++idim) {
        v0[idim]=getEnergyAttribute(actNode,std::string("v0")+
                                    std::string(dimName).substr(idim,1));
      }
      Vec length;
      for (int idim=0; idim<NDIM; ++idim) {
        length[idim]=getLengthAttribute(actNode,std::string("l")+
                                        std::string(dimName).substr(idim,1));
      }
      Vec max;
      for (int idim=0; idim<NDIM; ++idim) {
        max[idim]=getLengthAttribute(actNode,std::string("max")+
                                     std::string(dimName).substr(idim,1));
      }
      composite->addAction(new OpticalLatticeAction(v0,length,max,simInfo));
      continue;
    } else if (name=="PrimSHOAction") {
      double a=getDoubleAttribute(actNode,"a");
      double b=getDoubleAttribute(actNode,"b");
      double omega=getEnergyAttribute(actNode,"omega");
      double d=getLengthAttribute(actNode,"d");
      std::string specName=getStringAttribute(actNode,"species");
      const Species& species(simInfo.getSpecies(specName));
      const double mass = species.mass; 
      if (a==0.&&b==0.&&d>0.0001) { 
            a=-0.25*mass*omega*omega;
            b=0.5*mass*omega*omega/d/d;
         }
      if (a==0.&&b==0.&&d<0.0001) {
             a=0.5*mass*omega*omega;
             b=0;
         }
      std::cout <<"a,b=" <<a<<","<<b<<std::endl;
      int ndim=getIntAttribute(actNode,"ndim");
      if (ndim==0) ndim=NDIM;
      composite->addAction(new PrimSHOAction(a,b,simInfo,ndim,species));
      continue;
    } else if (name == "PrimImageAction") {
            double d = getLengthAttribute(actNode, "d");
            double del = getLengthAttribute(actNode, "del");
            double epsilon = getDoubleAttribute(actNode, "epsilon");
            double epsilonrel = getDoubleAttribute(actNode, "epsilonrel");
            double vbarr = getEnergyAttribute(actNode, "vbarr");
            std::string specName = getStringAttribute(actNode, "species");
            const Species& species(simInfo.getSpecies(specName));
            /*std::cout <<"d,epsilon,epsilonrel,vbarr="<<d<<","
              <<epsilon<<", "<<epsilonrel", "<<vbarr<<std::endl; */
            int ndim = getIntAttribute(actNode, "ndim");
            if (ndim == 0)
                ndim = NDIM;
            composite->addAction(
                    new PrimImageAction(d, del, epsilon, epsilonrel, vbarr,
                            simInfo, ndim, species));
            continue;

    } else if (name=="PrimCosineAction") {
	  double a=getDoubleAttribute(actNode,"a");
	  double b=getDoubleAttribute(actNode,"b");
	  int ndim=getIntAttribute(actNode,"ndim");
	  if (ndim==0) ndim=1;
	  composite->addAction(new PrimCosineAction(a,b,simInfo,ndim));
	  continue;
    } else if (name=="PrimTorusAction") {
	  double a=getDoubleAttribute(actNode,"a");
	  double b=getLengthAttribute(actNode,"b");
	  double c=getDoubleAttribute(actNode,"c");		
	  double omegar=getEnergyAttribute(actNode,"omegar");
	  double omegaz=getEnergyAttribute(actNode,"omegaz");
	  std::string specName=getStringAttribute(actNode,"species");
	  const Species& species(simInfo.getSpecies(specName));
	  const double mass = species.mass;
	  if (a==0) a=0.5*mass*omegar*omegar;
	  if (c==0) c=0.5*mass*omegaz*omegaz;
          int ndim=getIntAttribute(actNode,"ndim");
	  if (ndim==0) ndim=NDIM;
          composite->addAction(new PrimTorusAction(a,b,c,simInfo,ndim,species));
	  continue;
     } else if (name=="PrimShellAction") {
	  double a=getDoubleAttribute(actNode,"a");
	  double b=getDoubleAttribute(actNode,"b");
	  double omega=getEnergyAttribute(actNode,"omega");
	  std::string specName=getStringAttribute(actNode,"species");
	  const Species& species(simInfo.getSpecies(specName));
	  const double mass = species.mass;
	  if (a==0) a=0.5*mass*omega*omega;
          int ndim=getIntAttribute(actNode,"ndim");
	  if (ndim==0) ndim=3;
          composite->addAction(new PrimShellAction(a,b,simInfo,ndim,species));
	  continue;
     } else if (name=="PrimColloidalAction") {
          double B1=getLengthAttribute(actNode,"B1");
          double B2=getLengthAttribute(actNode,"B2");
          double V_lig=getEnergyAttribute(actNode,"V_ext");
          double V_cdte=getEnergyAttribute(actNode,"V_core");
          double V_cdse=getEnergyAttribute(actNode,"V_shell");
          std::string specName=getStringAttribute(actNode,"species");
          const Species& species(simInfo.getSpecies(specName));
          //const double mass = species.mass;
          int ndim=getIntAttribute(actNode,"ndim");
          if (ndim==0) ndim=3;
          composite->addAction(new PrimColloidalAction(B1,B2,V_lig,V_cdte,V_cdse,simInfo,ndim,species));
          continue;
      } else if (name=="PrimAnisSHOAction") {
		double a=getDoubleAttribute(actNode,"a");
		double b=getDoubleAttribute(actNode,"c");
		double c=getDoubleAttribute(actNode,"c");
		double omegax=getEnergyAttribute(actNode,"omegax");
		double omegay=getEnergyAttribute(actNode,"omegay");
		double omegaz=getEnergyAttribute(actNode,"omegaz");
		std::string specName=getStringAttribute(actNode,"species");
		const Species& species(simInfo.getSpecies(specName));
		const double mass = species.mass; 
		if (a==0)  a=0.5*mass*omegax*omegax;
		if (b==0) b=0.5*mass*omegay*omegay;
		if (c==0) c=0.5*mass*omegaz*omegaz;
		int ndim=getIntAttribute(actNode,"ndim");
		if (ndim==0) ndim=NDIM;
		composite->addAction(new PrimAnisSHOAction(a,b,c,simInfo,ndim,species));
		continue;
    } else if (name=="StillingerWeberAction") {
      composite->addAction(new StillWebAction(simInfo,"struct.h5"));
//    } else if (name=="GrapheneActionAction") {
//      composite->addAction(new GrapheneAction(simInfo));
    } else if (name=="SpinAction") {
      const double bx=getEnergyAttribute(actNode,"bx");
      const double by=getEnergyAttribute(actNode,"by");
      const double bz=getEnergyAttribute(actNode,"bz");
      const double gc=getEnergyAttribute(actNode,"gc");
      const double tau=simInfo.getTau();
      const double mass=simInfo.getSpecies(0).mass;
      composite->addAction(
        new SpinAction(tau,mass,bx,by,bz,simInfo.spinOmega,gc));
      continue;
 } else if (name=="SHOAction") {
      double omega=getEnergyAttribute(actNode,"omega");
      Vec center;
      for (int idim=0; idim<NDIM; ++idim) {
        center[idim]=getLengthAttribute(actNode,
                                  std::string(dimName).substr(idim,1));
      }
      if (omega==0) omega=1;
      int ndim=getIntAttribute(actNode,"ndim");
      if (ndim==0) ndim=NDIM;
      std::string specName=getStringAttribute(actNode,"species");
      const Species& species(simInfo.getSpecies(specName));
      const double mass = species.mass;
      composite->addAction(new SHOAction(simInfo.getTau(),
                               omega,mass,ndim,species,center));
      continue;
   }  else if (name=="TwoQDAction") {
      const double omega=getEnergyAttribute(actNode,"omega");
      const double mass=simInfo.getSpecies(0).mass;
      const double d=getLengthAttribute(actNode,"d");
      double alpha=getDoubleAttribute(actNode,"alpha");
      if (alpha==0.) alpha=1.0;
      composite->addAction(
        new TwoQDAction(simInfo.getTau(),omega,mass,d,alpha));
      continue;
    } else if (name=="QPCAction") {
      double d=getLengthAttribute(actNode,"d");
      if (d==0) d=780.46;
      double v0=getEnergyAttribute(actNode,"v0");
      if (v0==0) v0=0.00011025;
      bool isWireOnly=getBoolAttribute(actNode,"isWireOnly");
      if (isWireOnly) {d=v0=0;}
      double omega=getEnergyAttribute(actNode,"omega");
      if (omega==0) omega=0.0000735;
      const double mass=simInfo.getSpecies(0).mass;
      composite->addAction(new QPCAction(simInfo.getTau(),d,v0,omega,mass));
      continue;
    } else if (name=="TimpQPC") {
      double w=getLengthAttribute(actNode,"w"); if (w==0) w=189.;
      double l=getLengthAttribute(actNode,"l"); if (l==0) l=587.;
      double vG=getEnergyAttribute(actNode,"vG"); if (vG==0) vG=-0.03675;
      double z=getLengthAttribute(actNode,"z"); if (z==0) z=189.;
      std::string specName=getStringAttribute(actNode,"species");
      const Species& species(simInfo.getSpecies(specName));
      composite->addAction(new TimpQPC(*simInfo.getSuperCell(),species,
                                        simInfo.getTau(),w,l,vG,z,mpi));
    } else if (name=="JelliumSlab") {
      double qtot=getDoubleAttribute(actNode,"qtot");
      double thick=getLengthAttribute(actNode,"thickness");
      bool isSlabInMiddle=getBoolAttribute(actNode,"isSlabInMiddle");
      composite->addAction(new JelliumSlab(simInfo,qtot,thick,isSlabInMiddle));
    } else if (name=="SphereAction") {
      double radius=getLengthAttribute(actNode,"radius");
      std::string specName=getStringAttribute(actNode,"species");
      const Species& species(simInfo.getSpecies(specName));
      composite->addAction(new SphereAction(simInfo.getTau(),radius,species));
      continue;
    } else if (name=="GaussianDotAction") {
      double R=getLengthAttribute(actNode,"radius");
      double alpha=1./(R*R);
      Vec center;
      for (int idim=0; idim<NDIM; ++idim) {
        center[idim]=getLengthAttribute(actNode,
                                  std::string(dimName).substr(idim,1));
      }
      double v0=getEnergyAttribute(actNode,"v0");
      composite->addAction(new GaussianDotAction(v0,alpha,center,simInfo));
      continue;
    } else if (name=="SHODotAction") {
      double t=getLengthAttribute(actNode,"thickness");
      double z=getLengthAttribute(actNode,"z");
      double v0=getEnergyAttribute(actNode,"v0");
      double omega=getEnergyAttribute(actNode,"omega");
      std::string specName=getStringAttribute(actNode,"species");
      const Species& species(simInfo.getSpecies(specName));
      const bool semiclass=getBoolAttribute(actNode,"useSemiClassical");
      int numds=0;
      if(semiclass) {
        numds=getIntAttribute(actNode,"numds");
        //Default to 10 segements for ds in semiclassical action
        if(numds==0)
          numds=10;
      }
      composite->addAction(
        new SHODotAction(simInfo,t,v0,omega,z,species,semiclass,numds));
      continue;
    } else if (name=="DotGeomAction") {
      composite->addAction(new DotGeomAction(simInfo.getTau()));
      continue;
    } else if (name == "RingGateAction") {
      if (NDIM != 2) {
          std::cout<<"The RingGateAction only works for 2D."<<std::endl;
          exit(-1);
      }
      double GVolt = getEnergyAttribute(actNode,"Vg");
      double s = getDoubleAttribute(actNode,"s");
      double theta0 = getDoubleAttribute(actNode,"theta0");
      if (theta0 == 0) {
          std::cout<<"The ring gate action is shut down because theta0 = 0."<<std::endl;
      }
      std::string specName=getStringAttribute(actNode,"species");
      const Species& species(simInfo.getSpecies(specName));
      std::cout << "The ring gate action is set." << std::endl;
      composite->addAction(new RingGateAction(simInfo,GVolt,s,theta0,species));
      continue;
    } else if (name == "GateAction") {
      if (NDIM != 2) {
          std::cout<<"The GateAction only works for 2D."<<std::endl;
          exit(-1);
      }
      double GVolt = getEnergyAttribute(actNode, "Vg");
      double sx = getDoubleAttribute(actNode, "sx");
      double sy = getDoubleAttribute(actNode, "sy");
      double xwidth = getLengthAttribute(actNode, "xwidth");
      double ywidth = getLengthAttribute(actNode, "ywidth");
      double xoffset = getLengthAttribute(actNode, "xoffset");
      double yoffset = getLengthAttribute(actNode, "yoffset");
      std::string specName = getStringAttribute(actNode, "species");
      const Species& species(simInfo.getSpecies(specName));
      std::cout<<"The gate action is on."<<std::endl;
      composite->addAction(new GateAction(simInfo, GVolt, sx, sy, xwidth, 
                        ywidth, xoffset, yoffset, species));
      continue;
    } else if (name=="SpringTensorAction") {
      composite->addAction(new SpringTensorAction(simInfo));
      continue;
    }  else if (name=="FixedNodeAction") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species");
      const Species& species(simInfo.getSpecies(specName));
      bool noNodalAction=getBoolAttribute(actNode,"noNodalAction");
      bool useDistDerivative=getBoolAttribute(actNode,"useDistDerivative");
      bool useManyBodyDistance=getBoolAttribute(actNode,"useManyBodyDistance");
      int nerrorMax=getIntAttribute(actNode,"maxCross");

      //This is how we allow maxCross==0 b/c getIntAttribute defaults to 0
      xmlChar* spMaxCross = xmlCharStrdup("maxCross");
      if (!xmlHasProp(actNode, spMaxCross)) 
          nerrorMax=1000;
      if (spMaxCross) delete []spMaxCross;
      std::cout<<"Max Node Crossings: "<<nerrorMax<<std::endl;
      NodeModel *nodeModel = parseNodeModel(ctxt,actNode,species);
      doubleComposite->addAction(
          new FixedNodeAction(simInfo,species,nodeModel,!noNodalAction,
                              useDistDerivative,maxlevel,useManyBodyDistance,nerrorMax,mpi));
      continue;
    }  else if (name=="FixedPhaseAction") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species");
      std::string modelName=getStringAttribute(ctxt->node,"model");
      const Species& species(simInfo.getSpecies(specName));
      double t=getEnergyAttribute(actNode,"temperature");
      if (t==0) t=simInfo.getTemperature();
      PhaseModel *phaseModel=0;
      if (modelName=="SHOPhase") {
        double omega=getEnergyAttribute(actNode,"omega");
	double b=getEnergyAttribute(actNode,"b");
        phaseModel=new SHOPhase(simInfo,species,omega,t,b,maxlevel);
      }else if(modelName=="SpinPhase"){
        double t=getEnergyAttribute(actNode,"t");
        if (t==0) t=simInfo.getTemperature();
        double bx=getEnergyAttribute(actNode,"bx");
        double by=getEnergyAttribute(actNode,"by");
        double bz=getEnergyAttribute(actNode,"bz");
        double gmubs=getDoubleAttribute(actNode,"gmubs");
        if (gmubs==0) gmubs=1.0;
        phaseModel=new Spin4DPhase(simInfo,species,t,bx,by,bz,gmubs,maxlevel);
      }  	      
        doubleComposite->addAction(
                              new FixedPhaseAction(simInfo,species,phaseModel));
    }  else if (name=="SpinFixedPhaseAction") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species");
      std::string modelName=getStringAttribute(ctxt->node,"model");
      const Species& species(simInfo.getSpecies(specName));
      double t=getEnergyAttribute(actNode,"temperature");
      if (t==0) t=simInfo.getTemperature();
      double bx=getEnergyAttribute(actNode,"bx");
      double by=getEnergyAttribute(actNode,"by");
      double bz=getEnergyAttribute(actNode,"bz");
      double gmubs=getDoubleAttribute(actNode,"gmubs");
      if (gmubs==0) gmubs=1.0;
      SpinPhaseModel *phaseModel=0;
      if (modelName=="LatticeSpinPhase") {
        phaseModel=
          new LatticeSpinPhase(simInfo,species,t,bx,by,bz,gmubs,maxlevel);
      }else if(modelName=="SpinPhase") {
        phaseModel=new SpinPhase(simInfo,species,t,bx,by,bz,gmubs,maxlevel);
      }  	      
        doubleComposite->addAction(
                         new SpinFixedPhaseAction(simInfo,species,phaseModel));
    } else if (name=="FreePartNodesNoUpdate") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species");
      const Species& species(simInfo.getSpecies(specName));
      double t=getEnergyAttribute(actNode,"temperature");
      if (t==0) t=simInfo.getTemperature();
      doubleComposite->addAction(new FreePartNodesNoUpdate(simInfo,species,t));
      continue;
    } else if (name=="PairAction") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species1");
      const Species& species1(simInfo.getSpecies(specName));
      specName=getStringAttribute(ctxt->node,"species2");
      const Species& species2(simInfo.getSpecies(specName));
      std::string filename=getStringAttribute(ctxt->node,"file");
      int norder=getIntAttribute(actNode,"norder");
      bool isDMD=getBoolAttribute(actNode,"isDMD");
      bool hasZ=getBoolAttribute(actNode,"hasZ");
      bool readSquarerFile =getBoolAttribute(actNode,"squarerFile"); 
      if (readSquarerFile>0){
	std :: cout << "Pair action from squarer file"<< std :: endl;
	composite->addAction(new PairAction(species1,species2,filename,
					    simInfo,norder,hasZ,false,-1));
      } else {
	composite->addAction(new PairAction(species1,species2,filename,
					    simInfo,norder,hasZ,isDMD,-1));
      }

      continue;
    } else if (name=="EmpiricalInteraction") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species1");
      const Species& species1(simInfo.getSpecies(specName));
      specName=getStringAttribute(ctxt->node,"species2");
      const Species& species2(simInfo.getSpecies(specName));
      std::string modelName=getStringAttribute(ctxt->node,"model");
      PairPotential* pot=0;
      int norder=getIntAttribute(actNode,"norder");
      double rmin=getLengthAttribute(actNode,"rmin");
      double rmax=getLengthAttribute(actNode,"rmax");
      int ngpts=getIntAttribute(actNode,"ngridPoints");
      if (modelName=="cosh2") {
        double v0=getEnergyAttribute(actNode,"v0");
        double kappa=getInvLengthAttribute(actNode,"kappa");
        pot = new InverseCosh2Potential(v0,kappa);
      } else if (modelName=="LJ") {
        double epsilon=getEnergyAttribute(actNode,"epsilon");
        double sigma=getLengthAttribute(actNode,"sigma");
        pot = new LennardJonesPotential(epsilon,sigma);
      } else if (modelName=="Aziz") {
        pot = new AzizPotential();
      } else if (modelName=="CaoBerne") {
        double mu=1./(1./species1.mass+1./species2.mass);
        double radius = getLengthAttribute(actNode,"radius");
        bool dumpFiles=getBoolAttribute(actNode,"dumpFiles");
	    bool hasZ=getBoolAttribute(actNode,"hasZ");
        PairAction* action = new PairAction(species1,species2,
                             CaoBerneAction(mu,radius,tau,norder),
                             simInfo,norder,rmin,rmax,ngpts,true,-1);
        if (dumpFiles) action -> write("",hasZ);
        composite->addAction(action);
        continue; 
      }
      bool useIntegrator=getBoolAttribute(actNode,"useIntegrator");
      if (useIntegrator) {
        int norder=getIntAttribute(actNode,"norder");
        double deltar=getLengthAttribute(actNode,"deltaR");
        double tol=getDoubleAttribute(actNode,"tol");
        double intRange=getDoubleAttribute(actNode,"intRange");
        if (intRange<1.0) intRange = 2.5;
        int maxIter=getIntAttribute(actNode,"maxIter");
        int nsegment=getIntAttribute(actNode,"nsegment");
        if (nsegment==0) nsegment=1;
        bool dumpFiles=getBoolAttribute(actNode,"dumpFiles");
        double mu=1./(1./species1.mass+1./species2.mass);
        PairIntegrator integrator(simInfo.getTau(),mu,deltar,
                                  norder,maxIter,*pot,tol,nsegment,intRange);
        double ascat = pot->getScatteringLength(mu,rmax,0.01*deltar); 
        std::cout << "Scattering length = " << ascat << "a0, or " 
                  << ascat*0.0529177 << " nm." << std::endl;
	bool hasZ=getBoolAttribute(actNode,"hasZ");
        PairAction *action = new PairAction(species1,species2,integrator,
                                            simInfo,norder,rmin,rmax,ngpts,-1);
        if (dumpFiles) action -> write("", hasZ);
        composite->addAction(action);
      } else {
        PrimitivePairAction empAction(*pot,simInfo.getTau());
        std::cout << "Scattering length = " 
                  << empAction.getScatteringLength(species1,species2,
                     rmax,rmax/(100*ngpts)) << std::endl;
        composite->addAction(new PairAction(species1,species2,empAction,
                                    simInfo,norder,rmin,rmax,ngpts,false,-1));
      }
      delete pot;
      continue;
    } else if (name=="GroundStateWFNodes") {
      ctxt->node=actNode;
      std::string specName=getStringAttribute(ctxt->node,"species");
      const Species& species(simInfo.getSpecies(specName));
      specName=getStringAttribute(ctxt->node,"refSpecies");
      const Species& refSpecies(simInfo.getSpecies(specName));
      composite->addAction(new GroundStateWFNodes(species,refSpecies));
    } else if (name=="EFieldAction") {
      double strength = getFieldStrengthAttribute(actNode,"strength");
      if (strength==0.) strength=getDoubleAttribute(actNode,"scale");
      std::string dir=getStringAttribute(actNode,"dir");
      if (dir=="") dir=getStringAttribute(actNode,"component");
      int idir=NDIM-1; //default use z component
      if (dir=="x") idir=0;
      else if (dir=="y") idir=1;
      double width = getLengthAttribute(actNode,"width");
      if (width==0.) width = 0.25*simInfo.getSuperCell()->a[idir];
      double center = getLengthAttribute(actNode,"center");
      composite->addAction(
        new EFieldAction(simInfo, strength, center ,width, idir));
      continue;
    } else if (name=="EwaldAction" && NDIM==3) {
      ctxt->node=actNode;
      composite->addAction(parseEwaldActions(ctxt));
      continue;
    } else if (name=="WellImageAction") {
      double epsIn=getDoubleAttribute(actNode,"epsIn");
      double epsOut=getDoubleAttribute(actNode,"epsOut");
      double width=getDoubleAttribute(actNode,"width");
      double z0=getDoubleAttribute(actNode,"z0");
      double delta=getDoubleAttribute(actNode,"delta");
      composite->addAction(new WellImageAction(simInfo,epsIn,epsOut,width,
                                 z0,delta)); 
      continue;
    } else if (name=="EMARateAction") {
      std::string specName=getStringAttribute(actNode,"species1");
      const Species& species1(simInfo.getSpecies(specName));
      specName=getStringAttribute(actNode,"species2");
      const Species& species2(simInfo.getSpecies(specName));
      const double C=getDoubleAttribute(actNode,"c");
      EMARateAction* action
          = new EMARateAction(simInfo, &species1, &species2, C);
      if (getBoolAttribute(actNode, "useCoulomb")) {
          double epsilon = getDoubleAttribute(actNode, "epsilon");
          if (epsilon < 1e-15) epsilon = 1.0;
          const int norder = getIntAttribute(actNode, "norder");
          action->includeCoulombContribution(epsilon, norder);
      }
      composite->addAction(action);
      continue;
    } else if (name=="SpinChoiceFixedNodeAction") {
      std::string initstring = getStringAttribute(actNode,"initial");
      int initial = 0;
      if (initstring == "alternating") initial = 1; 
      std::string specName=getStringAttribute(ctxt->node,"species");
      const Species& species(simInfo.getSpecies(specName));
      bool noNodalAction=getBoolAttribute(actNode,"noNodalAction");
      bool useDistDerivative=getBoolAttribute(actNode,"useDistDerivative");
      bool useManyBodyDistance=getBoolAttribute(actNode,"useManyBodyDistance");
      NodeModel *nodeModel = parseNodeModel(ctxt,actNode,species);
      SpinChoiceFixedNodeAction *action 
        = new SpinChoiceFixedNodeAction(simInfo,initial,species,nodeModel,
	                    !noNodalAction,
                            useDistDerivative,maxlevel,useManyBodyDistance,mpi);
      actionChoice = action;
      doubleComposite->addAction(action);
      continue;
    } else if (name=="ActionChoice") {
      int imodel=getIntAttribute(actNode,"initial");
      ctxt->node=actNode;
      xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
      int naction=obj->nodesetval->nodeNr;
      std::cout << naction << std::endl;
      ActionChoice* choice=new ActionChoice(naction);
      DoubleActionChoice* doubleChoice=new DoubleActionChoice(naction);
      parseActions(ctxt,obj,choice,doubleChoice);
      xmlXPathFreeObject(obj);
      if (doubleChoice->getCount()==0) {
        delete doubleChoice;
        composite->addAction(choice);
        actionChoice = choice;
      } else {
        delete choice;
        doubleComposite->addAction(doubleChoice);
        actionChoice = doubleChoice;
      }
      EnumeratedModelState *modelState
        = dynamic_cast<EnumeratedModelState*>(&actionChoice->getModelState());
      modelState->setModelState(imodel);
      modelState->broadcastToMPIWorkers(mpi);

      continue;
    } else if (name=="ActionGroup") {
      ctxt->node=actNode;
      xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
      int naction=obj->nodesetval->nodeNr;
      CompositeAction* group = new CompositeAction(naction);
      CompositeDoubleAction* doubleGroup = new CompositeDoubleAction(naction);
      parseActions(ctxt,obj,group,doubleGroup);
      xmlXPathFreeObject(obj);
      if (group->getCount()>0) {
        composite->addAction(group);
      } else {
        delete group;
      }
      if (doubleGroup->getCount()>0) {
        doubleComposite->addAction(doubleGroup);
      } else {
        delete doubleGroup;
      }
    }
  }
}

Action* ActionParser::parseEwaldActions(const xmlXPathContextPtr& ctxt) {
  EwaldAction* ewald=0;
#if NDIM==3
  double rcut=getLengthAttribute(ctxt->node,"rcut");
  double kcut=getDoubleAttribute(ctxt->node,"kcut");
  xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"PairAction",ctxt);
  int naction=obj->nodesetval->nodeNr;
  ewald=new EwaldAction(simInfo,rcut,kcut,naction);
  for (int iaction=0; iaction<naction; ++iaction) {
    xmlNodePtr actNode=obj->nodesetval->nodeTab[iaction];
    std::string specName=getStringAttribute(actNode,"species1");
    const Species& species1(simInfo.getSpecies(specName));
    specName=getStringAttribute(actNode,"species2");
    const Species& species2(simInfo.getSpecies(specName));
    std::string filename=getStringAttribute(actNode,"file");
    int norder=getIntAttribute(actNode,"norder");
    ewald->addAction(new PairAction(species1,species2,filename,
                                    simInfo,norder,false,false));
  }
  xmlXPathFreeObject(obj);
  ewald->setup();
#endif
  return ewald;
}

void ActionParser::parseOrbitalDM(
    std::vector<const AtomicOrbitalDM*>& orbitals,
    const Species& fSpecies, const xmlXPathContextPtr& ctxt) {
  xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"*",ctxt);
  int norb=obj->nodesetval->nodeNr;
  for (int iorb=0; iorb<norb; ++iorb) {
    xmlNodePtr orbNode=obj->nodesetval->nodeTab[iorb];
    std::string name=getName(orbNode);
    std::string specName=getStringAttribute(orbNode,"species");
    const Species& species(simInfo.getSpecies(specName));
    const int ifirst = species.ifirst;
    const int npart = species.count;
    double Z=getDoubleAttribute(orbNode,"Z");
    double weight=getDoubleAttribute(orbNode,"weight");
    std::cout << "Nodal orbital: " << name << " on " << specName 
              << " with Z=" << Z << " and weight " << weight << std::endl;
    if (name=="Atomic1s") {
      orbitals.push_back(
        new Atomic1sDM(Z,ifirst,npart,fSpecies.count,weight));
    } else if (name=="Atomic2sp") {
      double pweight=getDoubleAttribute(orbNode,"pweight");
      orbitals.push_back(
        new Atomic2spDM(Z,ifirst,npart,fSpecies.count,
                                        pweight,weight));
    }
  }
}

NodeModel* ActionParser::parseNodeModel(const xmlXPathContextPtr& ctxt,
  xmlNodePtr &actNode, const Species &species) {
      NodeModel *nodeModel=0;
      bool useHungarian=getBoolAttribute(actNode,"useHungarian");
      int useIterations=getIntAttribute(actNode,"useIterations");
      double nodalFactor=getDoubleAttribute(actNode,"nodalFactor");
      if (nodalFactor<=0) nodalFactor=5.0;
      if (useIterations<0) useIterations=0;
      double t=getEnergyAttribute(actNode,"temperature");
      if (t==0) t=simInfo.getTemperature();
      std::string modelName=getStringAttribute(actNode,"model");
      if (modelName=="SHONodes") {
        const double omega=getEnergyAttribute(actNode,"omega");
        nodeModel=new SHONodes(simInfo,species,omega,t,maxlevel);
      } else if (modelName=="GSSNode" || modelName=="GroundStateSNode") {
        std::string centerName=getStringAttribute(actNode,"center");
        const int icenter=simInfo.getSpecies(centerName).ifirst;
        nodeModel=new GroundStateSNode(species,icenter,simInfo.getTau());
      } else if (modelName=="WireNodes") {
        const double omega=getEnergyAttribute(actNode,"omega");
        const bool updates=getBoolAttribute(actNode,"useUpdates");
        Vec center;
        for (int idim=0; idim<NDIM; ++idim) {
          center[idim]=getLengthAttribute(actNode,
                                    std::string(dimName).substr(idim,1));
        }
        int maxMovers=0;
        if (updates) maxMovers=3;
        nodeModel=new WireNodes(simInfo,species,omega,t,center,maxlevel,
                                updates,maxMovers);
      } else if (modelName=="ExcitonNodes") {
        std::string specName=getStringAttribute(actNode,"species1");
        const Species& species1(simInfo.getSpecies(specName));
        specName=getStringAttribute(actNode,"species2");
        const Species& species2(simInfo.getSpecies(specName));
        const double radius=getLengthAttribute(actNode,"radius");
        const bool updates=getBoolAttribute(actNode,"useUpdates");
        int maxMovers=3;
        nodeModel=new ExcitonNodes(simInfo,species1,species2,
                                   t,maxlevel,radius,updates,maxMovers);
      } else if (modelName=="AugmentedNodes") {
        const bool updates=getBoolAttribute(actNode,"useUpdates");
        const bool useHungarian=getBoolAttribute(actNode,"useHungarian");
        int maxMovers=3;
        std::vector<const AtomicOrbitalDM*> orbitals;
        parseOrbitalDM(orbitals, species, ctxt);
        nodeModel=new AugmentedNodes(simInfo,species,
            t,maxlevel,updates,maxMovers,orbitals,useHungarian);
      } else {
        const bool updates=getBoolAttribute(actNode,"useUpdates");
        int maxMovers=0;
        if (updates) maxMovers=3;
        nodeModel=new FreeParticleNodes(simInfo,species,t,maxlevel,updates,
                                        maxMovers,useHungarian,useIterations, nodalFactor);
      }
      return nodeModel;
}
const std::string ActionParser::dimName="xyzklmnopqrstuv";
