#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "WriteProbDensity.h"
#include "ProbDensityGrid.h"
#include "base/SimulationInfo.h"
#include <hdf5.h>
#include <cstdlib>
#include <blitz/array.h>
#include <iostream>

WriteProbDensity::WriteProbDensity(const SimulationInfo& simInfo,
  const ProbDensityGrid* grid, const std::string& filename)
  : filename(filename), grids(grid), name(simInfo.getNSpecies(),"rho") {
  for (unsigned int i=0; i<name.size(); ++i) {
    name[i]+=simInfo.getSpecies(i).name;
  }
}

WriteProbDensity::~WriteProbDensity() {}

void WriteProbDensity::run() {
  int rank=0;
  std::cout << "WARNING :: WriteProbDensity is obsolete. Please use DensityEstimator instead!!!" << std::endl;
#ifdef ENABLE_MPI
  rank=MPI::COMM_WORLD.Get_rank();
#endif
  //Output the density to output file.
  hid_t fileID = 0;
  if (rank==0) {
    fileID = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, 
                       H5P_DEFAULT);
  }
  IVec n=grids->getGridDim();
  const double norm=grids->getNorm(); double normsum=norm;
#ifdef ENABLE_MPI
  MPI::COMM_WORLD.Reduce(&norm,&normsum,1,MPI::DOUBLE,MPI::SUM,0);
#endif
  const double anorm=1.0/normsum;
  FArrayN floatgrid(n),sumgrid(n);
  for (unsigned int i=0; i<name.size(); ++i) {
    const LIArrayN& intgrid(grids->getGrid(i));
    floatgrid=1.0*intgrid;
    sumgrid=floatgrid;
#ifdef ENABLE_MPI
    MPI::COMM_WORLD.Reduce(floatgrid.data(),sumgrid.data(),product(n),
                           MPI::FLOAT,MPI::SUM,0);
#endif
    sumgrid*=anorm;
    if (rank==0) {
      hsize_t dims[NDIM];
      for (int idim=0; idim<NDIM; ++idim) dims[idim]=n[idim];
      //hid_t dataSpaceID = H5Screate_simple(3, dims, NULL);
      hid_t dataSpaceID = H5Screate_simple(NDIM, dims, NULL);
#if (H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))
      hid_t dataSetID = H5Dcreate2(fileID, name[i].c_str(), H5T_NATIVE_FLOAT,
                                   dataSpaceID, H5P_DEFAULT,
                                   H5P_DEFAULT, H5P_DEFAULT);
#else
      hid_t dataSetID = H5Dcreate(fileID, name[i].c_str(), H5T_NATIVE_FLOAT,
                                  dataSpaceID, H5P_DEFAULT);
#endif
      H5Dwrite(dataSetID, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT,
               sumgrid.data());
      {/// Write a few more attributes for use with DX ImportHDF5Field
        double a=grids->getLatticeConstant();
        Vec origin;
        for (int idim=0; idim<NDIM; ++idim) origin[idim]=-0.5*a*n[idim];
        hsize_t dims = NDIM;
        hid_t dataSpaceID = H5Screate_simple(1, &dims, NULL);
#if (H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))
        hid_t attrID = H5Acreate2(dataSetID, "origin", H5T_NATIVE_FLOAT,
                                  dataSpaceID, H5P_DEFAULT, H5P_DEFAULT);
#else
        hid_t attrID = H5Acreate(dataSetID, "origin", H5T_NATIVE_FLOAT,
                                 dataSpaceID, H5P_DEFAULT);
#endif
        H5Awrite(attrID, H5T_NATIVE_DOUBLE, &origin);
        H5Aclose(attrID);
        Vec delta; delta=a;
#if (H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))
        attrID = H5Acreate2(dataSetID, "delta", H5T_NATIVE_FLOAT,
                            dataSpaceID, H5P_DEFAULT, H5P_DEFAULT);
#else
        attrID = H5Acreate(dataSetID, "delta", H5T_NATIVE_FLOAT,
                           dataSpaceID, H5P_DEFAULT);
#endif
        H5Awrite(attrID, H5T_NATIVE_DOUBLE, &origin);
        H5Aclose(attrID);
        H5Sclose(dataSpaceID);
      }
      H5Sclose(dataSpaceID);
      H5Dclose(dataSetID);
    }
  }
  if (rank==0) {
    Vec delta; 
    delta=grids->getLatticeConstant(); 
    hsize_t dims=1;
    hid_t dataSpaceID = H5Screate_simple(1, &dims, NULL);
#if (H5_VERS_MAJOR>1)||((H5_VERS_MAJOR==1)&&(H5_VERS_MINOR>=8))
    hid_t dsetID = H5Dcreate2(fileID, "a", H5T_NATIVE_FLOAT,
                              dataSpaceID, H5P_DEFAULT, 
                              H5P_DEFAULT, H5P_DEFAULT);
#else
    hid_t dsetID = H5Dcreate(fileID, "a", H5T_NATIVE_FLOAT,
                             dataSpaceID, H5P_DEFAULT);
#endif
    H5Dwrite(dsetID, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &delta);
    H5Dclose(dsetID);
    H5Sclose(dataSpaceID);
    H5Fclose(fileID);
  }
#ifdef ENABLE_MPI
  MPI::COMM_WORLD.Barrier();
#endif
}
