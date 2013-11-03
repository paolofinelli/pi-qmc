#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "SimInfoParser.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "stats/Units.h"
#include "util/SuperCell.h"
#include <vector>
#include <blitz/tinyvec-et.h>

SimInfoParser::~SimInfoParser() {
  delete simInfo;
}

void SimInfoParser::parse(const xmlXPathContextPtr& ctxt) {
  if (!simInfo) simInfo = new SimulationInfo;
  simInfo->units=new Units("Ha","a0");
  this->units=simInfo->units;
  // Parse the species information.
  simInfo->npart=0;
  xmlXPathObjectPtr obj = xmlXPathEval(BAD_CAST"//Species",ctxt);
  int nspecies=obj->nodesetval->nodeNr;
  std::vector<Species*>& speciesList(simInfo->speciesList);
  speciesList.resize(nspecies);
  for (int ispecies=0; ispecies<nspecies; ++ispecies) {
    xmlNodePtr specNode=obj->nodesetval->nodeTab[ispecies];    
    Species *species=new Species();
    speciesList[ispecies]=species;
    species->name=getStringAttribute(specNode,"name");
    species->count=getIntAttribute(specNode,"count");
    species->mass=getMassAttribute(specNode,"mass"); 
    species->charge=getDoubleAttribute(specNode,"charge");
    species->twos=getIntAttribute(specNode,"twos");
    std::string type=getStringAttribute(specNode,"type");
    species->isFermion=(type=="fermion")?true:false;
    species->isStatic=getBoolAttribute(specNode,"isStatic");
    species->displace=getLengthAttribute(specNode,"displace");
    simInfo->npart+=species->count;
    // Look for optional child nodes.
    ctxt->node=specNode;
    xmlXPathObjectPtr obj2 = xmlXPathEval(BAD_CAST"AnisotropicMass",ctxt);
    if (obj2->nodesetval->nodeNr>0) {
      xmlNodePtr node=obj2->nodesetval->nodeTab[0];    
      species->anMass = new Vec(getVecAttribute(node,""));
    }
    xmlXPathFreeObject(obj2);
  }
  simInfo->speciesIndex.resize(simInfo->npart);
  int ipart=0;
  for (int ispecies=0; ispecies<nspecies; ++ispecies) {
    Species *species=speciesList[ispecies];
    species->ifirst=ipart;
    for (int i=0; i<species->count; ++i) {
      simInfo->speciesIndex[ipart++]=species;
    }
  }
  xmlXPathFreeObject(obj);
  // Parse the SuperCell information.
  obj = xmlXPathEval(BAD_CAST"//SuperCell",ctxt);
  xmlNodePtr node=obj->nodesetval->nodeTab[0]; ctxt->node=node;
  xmlXPathFreeObject(obj);
  const double a=getLengthAttribute(node,"a");
  Vec extent = getVecAttribute(node,"");
  simInfo->superCell=new SuperCell(a*extent);
  simInfo->superCell->computeRecipricalVectors();
  std::cout << "Supercell dimensions: "
            << simInfo->superCell->a << std::endl;
  // Parse the simulation temperature.
  obj = xmlXPathEval(BAD_CAST"//Temperature",ctxt);
  simInfo->temperature=getEnergyAttribute(obj->nodesetval->nodeTab[0],"value");
  simInfo->nslice=getIntAttribute(obj->nodesetval->nodeTab[0],"nslice");
  if (simInfo->nslice>0) {
    simInfo->tau=1.0/(simInfo->temperature * simInfo->nslice);
  } else {
    simInfo->tau=getTimeAttribute(obj->nodesetval->nodeTab[0],"tau");
    simInfo->nslice=(int)(1.0/(simInfo->temperature*simInfo->tau)+0.1);
  }
  xmlXPathFreeObject(obj);
}
