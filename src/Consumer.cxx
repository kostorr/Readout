// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Consumer.h"

int Consumer::pushData(DataSetReference &bc) {
  int success = 0;
  int error = 0;
  for (auto &b : *bc) {
    if (!pushData(b)) {
      success++;
    } else {
      error++;
    }
  }
  if (error) {
    totalPushError++;
    return -error; // return a negative number indicating number of errors
  }
  totalPushSuccess++;
  return success; // return a positive number indicating number of success
}
