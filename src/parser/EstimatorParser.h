// $Id$
/*  Copyright (C) 2004-2009 John B. Shumway, Jr.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
#ifndef __EstimatorParser_h_
#define __EstimatorParser_h_
#include <string>
#include <vector>
#include "parser/XMLUnitParser.h"
#include "estimator/PairCFEstimator.h"
#include "estimator/SpinChoicePCFEstimator.h"
class EstimatorManager;
class Distance;
class SimulationInfo;
class Action;
class ActionChoiceBase;
class DoubleAction;
class MPIManager;
class ScalarAccumulator;
/// XML Parser for estimators.
/// @version $Revision$
/// @author John Shumway */
class EstimatorParser {
public:
    typedef blitz::TinyVector<double, NDIM> Vec;
    typedef blitz::TinyVector<int, NDIM> IVec;
    /// Constructor.
    EstimatorParser(const SimulationInfo&, const double tau,
            const Action* action, const DoubleAction* doubleAction,
            ActionChoiceBase *actionChoice, MPIManager *mpi = 0);
    /// Virtual destructor.
    ~EstimatorParser();
    /// Parse some xml.
    void parse(const xmlXPathContextPtr& ctxt);
    /// Return the Estimator object.
    EstimatorManager* getEstimatorManager() {
        return manager;
    }
private:
    XMLUnitParser parser;
    /// The estimator manager.
    EstimatorManager* manager;
    /// General simulation information.
    const SimulationInfo& simInfo;
    /// The timestep.
    const double tau;
    /// The action.
    const Action* action;
    /// The double action.
    const DoubleAction* doubleAction;
    /// A choice of action models.
    ActionChoiceBase* actionChoice;
    /// The MPI manager.
    MPIManager *mpi;
    /// Parser for pair correlation estimator.
    template<int N> PairCFEstimator<N>* parsePairCF(xmlNodePtr estNode,
            xmlXPathObjectPtr ctxt);
    /// Parser for distance subtags.
    void parseDistance(xmlNodePtr estNode, const xmlXPathContextPtr& ctxt,
            std::vector<Distance*> &darray, std::vector<double>& min,
            std::vector<double> &max, std::vector<int>& nbin);
    /// Parser for distance subtags.
    void parsePairDistance(xmlNodePtr estNode, const xmlXPathContextPtr& ctxt,
            std::vector<PairDistance*> &darray, std::vector<double>& min,
            std::vector<double> &max, std::vector<int>& nbin);
    /// Parser for spin pairs.
    template<int N>
    SpinChoicePCFEstimator<N>* parseSpinPair(const std::string &name,
            const std::vector<double> &tmin, const std::vector<double> &tmax,
            const std::vector<int> &tnbin, 
	    const std::vector<PairDistance*> &dist, const int &ispin,
	    const bool &samespin);
};
#endif
