// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/*
  Create TPCFastTransform with average TPC space-charge correction map

  Authors: sergey.gorbunov@fias.uni-frankfurt.de
           hellbaer@ikf.uni-frankfurt.de

  Input space-charge density histograms can be found at
  /eos/user/e/ehellbar/SpaceCharge/data/RUN3/InputSCDensityHistograms

  Run macro:
  root -l -b -q $O2_SRC/Detectors/TPC/reconstruction/macro/createTPCSpaceChargeCorrection.C+\(\"InputSCDensityHistograms_8000events.root\",\"inputSCDensity3D_8000_0\",\"tpctransformSCcorrection.root\"\)

  Test macro compilation:
  .L $O2_SRC/Detectors/TPC/reconstruction/macro/createTPCSpaceChargeCorrection.C+
 */

#include <cmath>
#include <fstream>

#include "TCanvas.h"
#include "TFile.h"
#include "TH3.h"
#include "TLatex.h"

#include "TPCSpaceCharge/SpaceCharge.h"

#include "CommonConstants/MathConstants.h"
#include "CommonUtils/TreeStreamRedirector.h"
#include "TPCReconstruction/TPCFastTransformHelperO2.h"

using namespace o2;
using namespace tpc;
using namespace gpu;

/// \param nPhi number of phi bins of original correction lookup table
/// \param nR number of r bins of original correction lookup table, has to be 2^n + 1
/// \param nZ number of z bins of original correction lookup table, has to be 2^n + 1
const int nPhi = 180;
const int nR = 129;
const int nZ = 129;
using SC = o2::tpc::SpaceCharge<double>;
std::unique_ptr<SC> spaceCharge;

void getGlobalSpaceChargeCorrection(const int roc, double x, double y, double z,
                                    double& dx, double& dy, double& dz);
void initSpaceCharge(const char* histoFileName, const char* histoName);

void DumpFlatObjectToFile(const TPCFastTransform* obj, const char* file);
std::unique_ptr<TPCFastTransform> ReadFlatObjectFromFile(const char* file);

void debugInterpolation(utils::TreeStreamRedirector& pcstream, const o2::gpu::TPCFastTransformGeo& geo, TPCFastTransform* fastTransform);
void debugGridpoints(utils::TreeStreamRedirector& pcstream, const o2::gpu::TPCFastTransformGeo& geo, TPCFastTransform* fastTransform);

/// Creates TPCFastTransform object for TPC space-charge correction, stores it in a file and provides a debug tree if requested
/// \param histoFileName path and name to the root file containing input space-charge density histograms
/// \param histoName name of the input space-charge density histogram
/// \param outputFileName name of the output file to store the TPCFastTransform object in
/// \param debug create debug tree comparing original corrections and spline interpolations from TPCFastTransform (1 = on the spline interpolation grid, 2 = on the original lookup table grid)
void createTPCSpaceChargeCorrection(
  const char* histoFileName = "InputSCDensityHistograms_10000events.root",
  const char* histoName = "inputSCDensity3D_10000_avg",
  const char* outputFileName = "tpctransform.root",
  const int debug = 0)
{
  SC::setGrid(nZ, nR, nPhi);
  initSpaceCharge(histoFileName, histoName);
  TPCFastTransformHelperO2::instance()->setGlobalSpaceChargeCorrection(getGlobalSpaceChargeCorrection);

  std::unique_ptr<TPCFastTransform> fastTransform(TPCFastTransformHelperO2::instance()->create(0));

  fastTransform->writeToFile(outputFileName);

  if (debug > 0) {
    const o2::gpu::TPCFastTransformGeo& geo = fastTransform->getGeometry();
    utils::TreeStreamRedirector pcstream(TString::Format("fastTransformUnitTest_debug%d_gridsize%d-%d-%d.root", debug, nPhi, nR, nZ).Data(), "recreate");
    switch (debug) {
      case 1:
        debugInterpolation(pcstream, geo, fastTransform.get());
        break;
      case 2:
        debugGridpoints(pcstream, geo, fastTransform.get());
        break;
      default:
        printf("Debug option %d is not implemented. Debug tree will be empty.", debug);
    }
    pcstream.Close();
  }
}

/// Initialize calculation of original correction lookup tables
/// \param histoFileName path and name to the root file containing input space-charge density histograms
/// \param histoName name of the input space-charge density histogram
void initSpaceCharge(const char* histoFileName, const char* histoName)
{
  // get histogram with space-charge density
  std::unique_ptr<TFile> histoFile = std::unique_ptr<TFile>(TFile::Open(histoFileName));
  std::unique_ptr<TH3> scHisto = std::unique_ptr<TH3>((TH3*)histoFile->Get(histoName));

  // initialize space-charge object
  spaceCharge = std::make_unique<SC>();

  // input charge
  spaceCharge->fillChargeDensityFromHisto(*scHisto.get());

  // further parameters
  spaceCharge->setOmegaTauT1T2(0.32, 1.f, 1.f);

  // start calculation of lookup tables (takes some time)
  spaceCharge->calculateDistortionsCorrections(Side::A);
  spaceCharge->calculateDistortionsCorrections(Side::C);
}

/// Function to get corrections from original lookup tables
/// \param XYZ array with x, y and z position
/// \param dXdYdZ array with correction dx, dy and dz
void getGlobalSpaceChargeCorrection(const int roc, double x, double y, double z,
                                    double& dx, double& dy, double& dz)
{
  Side side = roc < 18 ? Side::A : Side::C;
  spaceCharge->getCorrections(x, y, z, side, dx, dy, dz);
}

/// Save TPCFastTransform to a file
/// \param obj TPCFastTransform object to store
/// \param file output file name
void DumpFlatObjectToFile(const TPCFastTransform* obj, const char* file)
{
  FILE* fp = fopen(file, "w+b");
  if (fp == nullptr) {
    return;
  }
  size_t size[2] = {sizeof(*obj), obj->getFlatBufferSize()};
  fwrite(size, sizeof(size[0]), 2, fp);
  fwrite(obj, 1, size[0], fp);
  fwrite(obj->getFlatBufferPtr(), 1, size[1], fp);
  fclose(fp);
}

/// Read TPCFastTransform from a file
/// \param file file name
/// \return TPCFastTransform object from file
std::unique_ptr<TPCFastTransform> ReadFlatObjectFromFile(const char* file)
{
  FILE* fp = fopen(file, "rb");
  if (fp == nullptr) {
    return nullptr;
  }
  size_t size0[2], r;
  r = fread(size0, sizeof(size0[0]), 2, fp);
  if (r == 0 || size0[0] != sizeof(TPCFastTransform)) {
    fclose(fp);
    return nullptr;
  }
  std::unique_ptr<TPCFastTransform> retVal(new TPCFastTransform);
  char* buf = new char[size0[1]]; // Not deleted as ownership is transferred to FlatObject
  r = fread((TPCFastTransform*)retVal.get(), 1, size0[0], fp);
  r = fread(buf, 1, size0[1], fp);
  fclose(fp);
  retVal->clearInternalBufferPtr();
  retVal->setActualBufferAddress(buf);
  retVal->adoptInternalBuffer(buf);
  return retVal;
}

/// Creates debug stream on spline interpolation grid to compare original corrections and spline interpolation
/// \param pcstream output stream
/// \param geo TPCFastTransformGeo object
/// \param fastTransform TPCFastTransform object
void debugInterpolation(utils::TreeStreamRedirector& pcstream,
                        const o2::gpu::TPCFastTransformGeo& geo,
                        TPCFastTransform* fastTransform)
{
  for (int slice = 0; slice < geo.getNumberOfSlices(); slice += 1) {
    // for (int slice = 21; slice < 22; slice += 1) {
    std::cout << "debug slice " << slice << " ... " << std::endl;
    const o2::gpu::TPCFastTransformGeo::SliceInfo& sliceInfo = geo.getSliceInfo(slice);

    for (int row = 0; row < geo.getNumberOfRows(); row++) {
      int nPads = geo.getRowInfo(row).maxPad + 1;

      for (int pad = 0; pad < nPads; pad++) {
        for (float time = 0; time < 500; time += 10) {

          // non-corrected point
          fastTransform->setApplyCorrectionOff();
          float lx, ly, lz;
          fastTransform->Transform(slice, row, pad, time, lx, ly, lz);
          float gx, gy, gz, r, phi;
          geo.convLocalToGlobal(slice, lx, ly, lz, gx, gy, gz);
          r = std::sqrt(lx * lx + ly * ly);
          phi = std::atan2(gy, gx);
          fastTransform->setApplyCorrectionOn();

          // fast transformation
          float lxT, lyT, lzT;
          fastTransform->Transform(slice, row, pad, time, lxT, lyT, lzT);
          float gxT, gyT, gzT, rT;
          geo.convLocalToGlobal(slice, lxT, lyT, lzT, gxT, gyT, gzT);
          rT = std::sqrt(lxT * lxT + lyT * lyT);

          // the original correction
          double gdC[3] = {0, 0, 0};
          Side side = slice < geo.getNumberOfSlicesA() ? Side::A : Side::C;
          spaceCharge->getCorrections(gx, gy, gz, side, gdC[0], gdC[1], gdC[2]);
          float ldxC, ldyC, ldzC;
          geo.convGlobalToLocal(slice, gdC[0], gdC[1], gdC[2], ldxC, ldyC, ldzC);

          double rC = std::sqrt((gx + gdC[0]) * (gx + gdC[0]) + (gy + gdC[1]) * (gy + gdC[1]));

          // calculate distortion for the xyz0
          double ldD[3] = {0.0, 0.0, 0.0};
          double gdD[3] = {0.0, 0.0, 0.0};

          float pointCyl[3] = {gz, r, phi};
          double efield[3] = {0.0, 0.0, 0.0};
          double charge = spaceCharge->getDensityCyl(pointCyl[0], pointCyl[1], pointCyl[2], side);
          double potential = spaceCharge->getPotentialCyl(pointCyl[0], pointCyl[1], pointCyl[2], side);
          spaceCharge->getElectricFieldsCyl(pointCyl[0], pointCyl[1], pointCyl[2], side, efield[0], efield[1], efield[2]);
          spaceCharge->getLocalDistortionsCyl(pointCyl[0], pointCyl[1], pointCyl[2], side, ldD[0], ldD[1], ldD[2]);
          spaceCharge->getDistortions(gx, gy, gz, side, gdD[0], gdD[1], gdD[2]);

          double gD[3] = {gx + gdD[0], gy + gdD[1], gz + gdD[2]};
          float rD = std::sqrt(gD[0] * gD[0] + gD[1] * gD[1]);

          // correction for the distorted point
          double gdDC[3] = {0, 0, 0};
          spaceCharge->getCorrections(gD[0], gD[1], gD[2], side, gdDC[0], gdDC[1], gdDC[2]);

          pcstream << "fastTransform"
                   // internal coordinates
                   << "slice=" << slice
                   << "row=" << row
                   << "pad=" << pad
                   << "time=" << time
                   // local coordinates
                   << "lx=" << lx
                   << "ly=" << ly
                   << "lz=" << lz
                   // global coordinates
                   << "gx=" << gx
                   << "gy=" << gy
                   << "gz=" << gz
                   // global r, phi
                   << "r=" << r
                   << "phi=" << phi
                   // local coordinates, corrected by the fast transformation
                   << "lxT=" << lxT
                   << "lyT=" << lyT
                   << "lzT=" << lzT
                   // global coordinates, corrected by the fast transformation
                   << "gxT=" << gxT
                   << "gyT=" << gyT
                   << "gzT=" << gzT
                   << "rT=" << rT
                   // original correction in the local coordinates
                   << "ldxC=" << ldxC
                   << "ldyC=" << ldyC
                   << "ldzC=" << ldzC
                   // original correction in the global coordinates
                   << "gdxC=" << gdC[0]
                   << "gdyC=" << gdC[1]
                   << "gdzC=" << gdC[2]
                   << "rC=" << rC
                   // original distortion in the global coordinates
                   << "gdxD=" << gdD[0]
                   << "gdyD=" << gdD[1]
                   << "gdzD=" << gdD[2]
                   << "rD=" << rD
                   // original local distortion (integrated over 1 z bin instead of full electron drift line) in the global coordinates
                   << "ldxD=" << ldD[0]
                   << "ldyD=" << ldD[1]
                   << "ldzD=" << ldD[2]
                   // original correction of the distorted point
                   << "gdxDC=" << gdDC[0]
                   << "gdyDC=" << gdDC[1]
                   << "gdzDC=" << gdDC[2]
                   //
                   << "charge=" << charge
                   << "potential=" << potential
                   << "Ez=" << efield[0]
                   << "Er=" << efield[1]
                   << "Ephi=" << efield[2]
                   << "\n";
        }
      }
    }
  }
}

/// Creates debug stream tcomparing original corrections and spline interpolation on the original grid
/// \param pcstream output stream
/// \param geo TPCFastTransformGeo object
/// \param fastTransform TPCFastTransform object
void debugGridpoints(utils::TreeStreamRedirector& pcstream, const o2::gpu::TPCFastTransformGeo& geo, TPCFastTransform* fastTransform)
{
  for (int iside = 0; iside < 2; ++iside) {
    const Side side = iside == 0 ? Side::A : Side::C;
    for (int iphi = 0; iphi < nPhi; ++iphi) {
      float phi = spaceCharge->getPhiVertex(iphi, side);
      int sector = iside == 0 ? phi / o2::constants::math::TwoPI * 18 : phi / o2::constants::math::TwoPI * 18 + 18;

      for (int ir = 0; ir < nR; ++ir) {
        float radius = spaceCharge->getRVertex(ir, side);
        float gx0 = radius * std::cos(phi);
        float gy0 = radius * std::sin(phi);

        for (int iz = 0; iz < nZ; ++iz) {
          float gz0 = spaceCharge->getZVertex(iz, side);
          float x0 = 0.f, y0 = 0.f, z0 = 0.f;
          geo.convGlobalToLocal(sector, gx0, gy0, gz0, x0, y0, z0);
          if (x0 < geo.getRowInfo(0).x - 0.375) {
            continue;
          }
          if (x0 >= (geo.getRowInfo(62).x + 0.375) && x0 < (geo.getRowInfo(63).x - 0.5)) {
            continue;
          }
          if (x0 >= (geo.getRowInfo(96).x + 0.5) && x0 < (geo.getRowInfo(97).x - 0.6)) {
            continue;
          }
          if (x0 >= (geo.getRowInfo(126).x + 0.6) && x0 < (geo.getRowInfo(127).x - 0.75)) {
            continue;
          }
          if (x0 > (geo.getRowInfo(151).x + 0.75)) {
            continue;
          }

          int row = 0;
          for (int irow = 0; irow < geo.getNumberOfRows(); ++irow) {
            row = irow;
            float rowX = geo.getRowInfo(irow).x;
            float rowLength2 = 0.375;
            if (irow >= 63) {
              rowLength2 = 0.5;
            }
            if (irow >= 97) {
              rowLength2 = 0.6;
            }
            if (irow >= 127) {
              rowLength2 = 0.75;
            }
            float xlow = rowX - rowLength2;
            float xhigh = rowX + rowLength2;
            if (x0 >= xlow && x0 < xhigh) {
              break;
            }
          }
          float u = 0.f, v = 0.f;
          geo.convLocalToUV(sector, y0, z0, u, v);
          float pad = 0.f, time = 0.f;
          fastTransform->convUVtoPadTime(sector, row, u, v, pad, time, 0.f);
          if (pad < 0) {
            continue;
          }

          fastTransform->setApplyCorrectionOn();
          float x1 = 0.f, y1 = 0.f, z1 = 0.f;
          fastTransform->Transform(sector, row, pad, time, x1, y1, z1);

          float gx1, gy1, gz1;
          geo.convLocalToGlobal(sector, x1, y1, z1, gx1, gy1, gz1);

          double gdx, gdy, gdz;
          getGlobalSpaceChargeCorrection(sector, gx0, gy0, gz0, gdx, gdy, gdz);
          float ldx, ldy, ldz;
          geo.convGlobalToLocal(sector, gdx, gdy, gdz, ldx, ldy, ldz);

          float pointCyl[3] = {gz0, radius, phi};
          double efield[3] = {0.0, 0.0, 0.0};
          double distLocal[3] = {0.0, 0.0, 0.0};
          double dist[3] = {0.0, 0.0, 0.0};
          double charge = spaceCharge->getDensityCyl(pointCyl[0], pointCyl[1], pointCyl[2], side);
          double potential = spaceCharge->getPotentialCyl(pointCyl[0], pointCyl[1], pointCyl[2], side);
          spaceCharge->getElectricFieldsCyl(pointCyl[0], pointCyl[1], pointCyl[2], side, efield[0], efield[1], efield[2]);
          spaceCharge->getLocalDistortionsCyl(pointCyl[0], pointCyl[1], pointCyl[2], side, distLocal[0], distLocal[1], distLocal[2]);
          spaceCharge->getDistortions(gx0, gy0, gz0, side, dist[0], dist[1], dist[2]);

          pcstream << "fastTransform"
                   // internal coordinates
                   << "slice=" << sector
                   << "row=" << row
                   << "pad=" << pad
                   << "time=" << time
                   // local coordinates
                   << "x0=" << x0
                   << "y0=" << y0
                   << "z0=" << z0
                   // local corrected coordinates
                   << "x1=" << x1
                   << "y1=" << y1
                   << "z1=" << z1
                   // global coordinates
                   << "gx0=" << gx0
                   << "gy0=" << gy0
                   << "gz0=" << gz0
                   // global corrected coordinates
                   << "gx1=" << gx1
                   << "gy1=" << gy1
                   << "gz1=" << gz1
                   // corrections local
                   << "lcorrdx=" << ldx
                   << "lcorrdy=" << ldy
                   << "lcorrdz=" << ldz
                   // corrections global
                   << "gcorrdx=" << gdx
                   << "gcorrdy=" << gdy
                   << "gcorrdz=" << gdz
                   // distortions
                   << "distdx=" << dist[0]
                   << "distdy=" << dist[1]
                   << "distdz=" << dist[2]
                   // local distortions
                   << "dxlocal=" << distLocal[0]
                   << "dylocal=" << distLocal[1]
                   << "dzlocal=" << distLocal[2]
                   //
                   << "charge=" << charge
                   << "potential=" << potential
                   << "Ez=" << efield[0]
                   << "Er=" << efield[1]
                   << "Ephi=" << efield[2]
                   << "\n";
        }
      }
    }
  }
}

/// Make a simple QA plot of the debug stream
void makeQAplot()
{
  TFile f("fastTransformUnitTest_debug1_gridsize180-129-129.root");
  TTree* tree = (TTree*)f.Get("fastTransform");
  tree->SetMarkerSize(0.2);
  tree->SetMarkerStyle(21);

  TLatex latex;
  latex.SetTextFont(43);
  latex.SetTextSizePixels(30);

  auto* c = new TCanvas("c", "c", 1200, 1000);
  c->Divide(2, 3);
  c->cd(1);
  tree->Draw("gxT-gx-gdxC:phi:r", "gz>0 && rC>83.5 && rC<254.5", "colz");
  latex.DrawLatexNDC(0.1, 0.92, "A side");
  c->cd(3);
  tree->Draw("gyT-gy-gdyC:phi:r", "gz>0 && rC>83.5 && rC<254.5", "colz");
  c->cd(5);
  tree->Draw("gzT-gz-gdzC:phi:r", "gz>0 && rC>83.5 && rC<254.5", "colz");
  c->cd(2);
  tree->Draw("gxT-gx-gdxC:phi:r", "gz<0 && rC>83.5 && rC<254.5", "colz");
  latex.DrawLatexNDC(0.1, 0.92, "C side");
  c->cd(4);
  tree->Draw("gyT-gy-gdyC:phi:r", "gz<0 && rC>83.5 && rC<254.5", "colz");
  c->cd(6);
  tree->Draw("gzT-gz-gdzC:phi:r", "gz<0 && rC>83.5 && rC<254.5", "colz");
  c->SaveAs("qaPlotTPCFastTransformSCcorrection.png");
}
