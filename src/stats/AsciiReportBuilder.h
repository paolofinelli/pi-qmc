#ifndef __AsciiReportBuilder_h_
#define __AsciiReportBuilder_h_

#include "EstimatorReportBuilder.h"
#include <fstream>
#include <string>

class ReportWriters;
class AsciiScalarReportWriter;

/** Class for reporting estimators to an ascii column file.
 @author John Shumway */
class AsciiReportBuilder: public EstimatorReportBuilder {
public:
    AsciiReportBuilder(const std::string & filename);
    virtual ~AsciiReportBuilder();

    virtual void initializeReport(EstimatorManager*);
    virtual void collectAndWriteDataBlock(EstimatorManager*);

private:
    std::string filename;
    std::ofstream file;
    int nstep;
    int istep;

    ReportWriters *reportWriters;
    AsciiScalarReportWriter *scalarWriter;
};
#endif
