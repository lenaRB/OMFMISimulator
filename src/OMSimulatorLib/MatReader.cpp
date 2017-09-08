/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-CurrentYear, Open Source Modelica Consortium (OSMC),
 * c/o Linköpings universitet, Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF GPL VERSION 3 LICENSE OR
 * THIS OSMC PUBLIC LICENSE (OSMC-PL) VERSION 1.2.
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES
 * RECIPIENT'S ACCEPTANCE OF THE OSMC PUBLIC LICENSE OR THE GPL VERSION 3,
 * ACCORDING TO RECIPIENTS CHOICE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from OSMC, either from the above address,
 * from the URLs: http://www.ida.liu.se/projects/OpenModelica or
 * http://www.openmodelica.org, and in the OpenModelica distribution.
 * GNU version 3 is obtained from: http://www.gnu.org/copyleft/gpl.html.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */

#include "MatReader.h"
#include "MatVer4.h"

#include "Logging.h"
#include "Util.h"

#include <string.h>

MatReader::MatReader(const char* filename)
{
  FILE *pFile = fopen(filename, "rb");
  skipMatVer4Matrix(pFile);
  name = readMatVer4Matrix(pFile);
  skipMatVer4Matrix(pFile);
  dataInfo = readMatVer4Matrix(pFile);
  data_1 = readMatVer4Matrix(pFile);
  data_2 = readMatVer4Matrix(pFile);
  fclose(pFile);
}

MatReader::~MatReader()
{
  deleteMatVer4Matrix(&name);
  deleteMatVer4Matrix(&dataInfo);
  deleteMatVer4Matrix(&data_1);
  deleteMatVer4Matrix(&data_2);
}

MatReader::Series* MatReader::getSeries(const char* var)
{
  // find index
  int index = -1;
  for (int i = 0; i < name->header.ncols; ++i)
    if (!strcmp(var, (char*)name->data + name->header.mrows*i))
      index = i;

  if (index == -1)
    return NULL;

  int32_t info[4];
  memcpy(&info, (int32_t*)dataInfo->data + 4 * index, 4 * sizeof(int32_t));

  if (info[1] < 0)
    return NULL;

  MatVer4Matrix *data = NULL;
  if (info[0] == 1)
    data = data_1;
  else if (info[0] == 2)
    data = data_2;
  else
    return NULL;

  Series *series = new Series;

  series->length = data->header.ncols;
  series->time = new double[series->length];
  series->value = new double[series->length];

  for (int i = 0; i < series->length; ++i)
  {
    series->time[i] = ((double*)data->data)[data->header.mrows * i];
    series->value[i] = ((double*)data->data)[data->header.mrows * i + (info[1] - 1)];
  }

  return series;
}

void MatReader::deleteSeries(Series** series)
{
  if (*series)
  {
    if ((*series)->time)
      delete[](*series)->time;
    if ((*series)->value)
      delete[](*series)->value;
    delete *series;
    *series = NULL;
  }
}

bool MatReader::compareSeries(Series* seriesA, Series* seriesB)
{
  if (!seriesA || !seriesA->time || !seriesA->value || seriesA->length < 2 ||
    !seriesB || !seriesB->time || !seriesB->value || seriesB->length < 2)
  {
    logError("MatReader::compareSeries: invalid input");
    return false;
  }

  unsigned int lengthA = seriesA->length;
  unsigned int lengthB = seriesB->length;

  if (!almostEqualRelativeAndAbs(seriesA->time[0], seriesB->time[0]))
  {
    logWarning("MatReader::compareSeries: start times are different");
    return false;
  }

  if (!almostEqualRelativeAndAbs(seriesA->time[seriesA->length - 1], seriesB->time[seriesB->length - 1]))
  {
    logWarning("MatReader::compareSeries: stop times are different");
    return false;
  }

  if (seriesA->time[0] >= seriesA->time[seriesA->length - 1] ||
    seriesB->time[0] >= seriesB->time[seriesB->length - 1])
  {
    logWarning("MatReader::compareSeries: invalid time frame");
    return false;
  }

  int iA = 0;
  int iB = 0;
  while (seriesA->time[iA] >= seriesA->time[iA + 1])
    iA++;
  while (seriesB->time[iB] >= seriesB->time[iB + 1])
    iB++;

  double t;
  double stopTime = fmin(seriesA->time[seriesA->length - 1], seriesB->time[seriesB->length - 1]);

  double mA, bA, valueA, valueA1, valueA2, timeA1, timeA2;
  double mB, bB, valueB, valueB1, valueB2, timeB1, timeB2;

  do
  {
    valueA1 = seriesA->value[iA];
    valueA2 = seriesA->value[iA + 1];
    timeA1 = seriesA->time[iA];
    timeA2 = seriesA->time[iA + 1];

    valueB1 = seriesB->value[iB];
    valueB2 = seriesB->value[iB + 1];
    timeB1 = seriesB->time[iB];
    timeB2 = seriesB->time[iB + 1];

    t = fmax(timeA1, timeB1);

    mA = (valueA2 - valueA1) / (timeA2 - timeA1);
    bA = valueA1 - mA*timeA1;

    mB = (valueB2 - valueB1) / (timeB2 - timeB1);
    bB = valueB1 - mB*timeB1;

    valueA = mA*t + bA;
    valueB = mB*t + bB;
    if (!almostEqualRelativeAndAbs(valueA, valueB))
    {
      logWarning("MatReader::compareSeries: different values at time " + std::to_string(t));
      return false;
    }

    if (almostEqualRelativeAndAbs(timeA2, timeB2))
    {
      do iA++; while (iA < lengthA-1 && seriesA->time[iA] >= seriesA->time[iA + 1]);
      do iB++; while (iB < lengthB-1 && seriesB->time[iB] >= seriesB->time[iB + 1]);
    }
    else if (timeA2 < timeB2)
      do iA++; while (iA < lengthA-1 && seriesA->time[iA] >= seriesA->time[iA + 1]);
    else
      do iB++; while (iB < lengthB-1 && seriesB->time[iB] >= seriesB->time[iB + 1]);

  } while (iA < lengthA && iB < lengthB);

  return true;
}
