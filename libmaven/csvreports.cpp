#include "csvreports.h"

CSVReports::CSVReports(vector<mzSample*>&insamples) {
    samples = insamples;
    groupId = 0;
    setUserQuantType(PeakGroup::AreaTop);
    setTabDelimited();
    sort(samples.begin(), samples.end(), mzSample::compSampleOrder);
}

CSVReports::~CSVReports() {
    closeFiles();
}

QString CSVReports::sanitizeString(const char* s) {
    //Merged with Maven776 - Kiran
    QString out=s;
    out.replace(QString("\""),QString("\"\""));
    if(out.contains(SEP.c_str())){
        out="\""+out+"\"";
    }
    return out;
}

void CSVReports::openGroupReport(string outputfile,bool includeSetNamesLine = false) {

    initialCheck(outputfile);
    openGroupReportCSVFile(outputfile);
    insertGroupReportColumnNamesintoCSVFile(outputfile, includeSetNamesLine);

}

void CSVReports::openPeakReport(string outputfile) {

    initialCheck(outputfile);
    openPeakReportCSVFile(outputfile);
    insertPeakReportColumnNamesintoCSVFile();

}

void CSVReports::initialCheck(string outputfile) {

    if (samples.size() == 0)
        return;
    if (QString(outputfile.c_str()).endsWith(".csv", Qt::CaseInsensitive))
        setCommaDelimited();
}

void CSVReports::openGroupReportCSVFile(string outputfile) {

     groupReport.open(outputfile.c_str());
}

void CSVReports::openPeakReportCSVFile(string outputfile) {

     peakReport.open(outputfile.c_str());
}

void CSVReports::insertGroupReportColumnNamesintoCSVFile(string outputfile,bool includeSetNamesLine){

    if (groupReport.is_open()) {
        QStringList groupReportcolnames;
        groupReportcolnames << "label" << "metaGroupId" << "groupId" << "goodPeakCount"
                << "medMz" << "medRt" << "maxQuality" << "note" << "compound"
                << "compoundId" << "expectedRtDiff" << "ppmDiff" << "parent";
        QString header = groupReportcolnames.join(SEP.c_str());
        groupReport << header.toStdString();
        for (unsigned int i = 0; i < samples.size(); i++) {
            groupReport << SEP << sanitizeString(samples[i]->sampleName.c_str()).toStdString();
        }
        groupReport << endl;
        if (includeSetNamesLine){
             for(unsigned int i=0; i < 12; i++) { groupReport << SEP; }
             for(unsigned int i=0; i< samples.size(); i++) { groupReport << SEP << sanitizeString(samples[i]->getSetName().c_str()).toStdString(); }
             groupReport << endl;
         }
    }
    else {
         cerr << "Error: Can't write to: " << outputfile << endl;
    }
}

void CSVReports::insertPeakReportColumnNamesintoCSVFile(){

    if (peakReport.is_open()) {
        QStringList peakReportcolnames;
        peakReportcolnames << "groupId" << "compound" << "compoundId" << "sample" << "peakMz"
                << "medianMz" << "baseMz" << "rt" << "rtmin" << "rtmax" << "quality"
                << "peakIntensity" << "peakArea" << "peakAreaTop"
                << "peakAreaCorrected" << "noNoiseObs" << "signalBaseLineRatio"
                << "fromBlankSample";
        QString header = peakReportcolnames.join(SEP.c_str());
        peakReport << header.toStdString() << endl;
    }
}

//Feng note: CSVReports::addGroup modified to (1) output only C12 data without labeling or wen compound is unknown, (2) output all related isotopic forms with labeling,
//even when peak height is zero

void CSVReports::addGroup (PeakGroup* group) {

      insertPeakInformationIntoCSVFile(group);

      if(!groupReport.is_open()) {
		return;
	  }
      //get ionization mode
      insertGroupInformationIntoCSVFile(group);

}

void CSVReports::insertPeakInformationIntoCSVFile(PeakGroup* group) {

      writePeakInfo(group);

}

void CSVReports::insertGroupInformationIntoCSVFile (PeakGroup* group) {

     //if compound is unknown, output only the unlabeled form information
    if (group->compound == NULL || group->childCount() == 0){

        writeGroupInfo(group);

    }

    //output all relevant isotope info otherwise
    else {

        writeGroupInfo(&group->children[0]); //C12 info

        int ionizationMode = getIonisationMode();

        vector<Isotope> masslist = computeIsotopes(group, ionizationMode);

        insertIsotopes(group,masslist);

    }
}

int CSVReports::getIonisationMode() {

      int ionizationMode;

      if (samples.size() > 0 && samples[0]->getPolarity() > 0)
          ionizationMode = +1;
      else
          ionizationMode = -1;

      return ionizationMode;
}



vector<Isotope> CSVReports::computeIsotopes (PeakGroup* group, int ionizationMode) {

      MassCalculator *masscalc;
      string formula = group->compound->formula;
      vector<Isotope> masslist = masscalc->computeIsotopes(formula,
              ionizationMode);

      return masslist;
}

void CSVReports::insertIsotopes (PeakGroup* group, vector<Isotope> masslist) {

      for (int i = 0; i < masslist.size(); i++) {
                Isotope& x = masslist[i];
                string isotopeName = x.name;
                if ((isotopeName.find("C13-label") != string::npos
                        && samples[0]->_C13Labeled == true)
                        || (isotopeName.find("N15-label") != string::npos
                                && samples[0]->_N15Labeled == true)
                        || (isotopeName.find("S34-label") != string::npos
                                && samples[0]->_S34Labeled == true)
                        || (isotopeName.find("D2-label") != string::npos
                                && samples[0]->_D2Labeled == true)) {
                    insertUserSelectedIsotopes(group,isotopeName);
                }
      }
}

void CSVReports::insertUserSelectedIsotopes(PeakGroup* group, string isotopeName) {

      int counter = 0;
      for (unsigned int k = 0; k < group->children.size() && counter == 0; k++) {
          //output non-zero-intensity peaks
          counter = insertIsotpesFoundInSamples(group, isotopeName, counter, k);
      }
      /*
       * Commented out in Maven776 - Kiran
      if (counter == 0) {
          //output zero-intensity peaks
          insertIsotpesNotFoundInSamples(group, isotopeName);
      }
      */
}

int CSVReports::insertIsotpesFoundInSamples (PeakGroup* group, string isotopeName, int counter, int k) {

      PeakGroup* subgroup = &group->children[k];
      if (subgroup->tagString == isotopeName) {
          subgroup->metaGroupId = group->metaGroupId;
          writeGroupInfo(subgroup);
          counter = 1;
      }
      return counter;
}

/*
 * Merged with Maven776 - Kiran
void CSVReports::insertIsotpesNotFoundInSamples (PeakGroup* group, string isotopeName) {

      groupReport << "No peak" << SEP << setprecision(7)
                  << group->metaGroupId << SEP << "N/A" << SEP
                  << "N/A" << SEP << "N/A" << SEP << "N/A" << SEP
                  << "N/A" << SEP << isotopeName << SEP
                  << group->compound->name << SEP
                  << group->compound->id << SEP << "N/A" << SEP
                  << "N/A";

       if (group->parent != NULL)
       groupReport << SEP << group->parent->meanMz;
       else
           groupReport << SEP << group->meanMz;
       for (unsigned int j = 0; j < samples.size(); j++)
           groupReport << SEP << 0;
       groupReport << endl;

}
*/
/*
 void CSVReports::addGroup(PeakGroup* group) {
 writeGroupInfo(group);
 writePeakInfo(group);
 }
 */

void CSVReports::closeFiles() {
    if (groupReport.is_open())
        groupReport.close();
    if (peakReport.is_open())
        peakReport.close();
}

void CSVReports::writeGroupInfo(PeakGroup* group) {
    if (!groupReport.is_open())
        return;
    groupId++;
    vector<float> yvalues = group->getOrderedIntensityVector(samples, qtype);
    //if ( group->metaGroupId == 0 ) { group->metaGroupId=groupId; }

    string tagString = group->srmId + group->tagString;
    // using the new funtionality added - Kiran
    tagString = sanitizeString(tagString.c_str()).toStdString();
    char label[2];
    sprintf(label, "%c", group->label);

    groupReport << label << SEP << setprecision(7) << group->metaGroupId << SEP
            << groupId << SEP << group->goodPeakCount << SEP << group->meanMz
            << SEP << group->meanRt << SEP << group->maxQuality << SEP
            << tagString;

    string compoundName;
    string compoundID;
    // TODO: Added this while merging this file
    string categoryString;
    float expectedRtDiff = 0;
    float ppmDist = 0;

    if (group->compound != NULL) {
        // TODO: Added this while merging this file
        compoundName = sanitizeString(group->compound->name.c_str()).toStdString();
        // TODO: Added this while merging this file
        compoundID   = sanitizeString(group->compound->id.c_str()).toStdString();
        ppmDist = mzUtils::ppmDist((double) group->compound->mass,
                (double) group->meanMz);
        expectedRtDiff = group->expectedRtDiff;

        // TODO: Added this while merging this file
        //for(int i=0;i<group->compound->category.size(); i++) {
        //    categoryString += group->compound->category[i] + ";";
        //}
        //categoryString=sanitizeString(categoryString.c_str()).toStdString();

    }

    groupReport << SEP << compoundName;
    groupReport << SEP << compoundID;
    //groupReport << SEP << categoryString;
    groupReport << SEP << expectedRtDiff;
    groupReport << SEP << ppmDist;

    if (group->parent != NULL) {
        groupReport << SEP << group->parent->meanMz;
    } else {
        groupReport << SEP << group->meanMz;
    }

    for (unsigned int j = 0; j < samples.size(); j++)
        groupReport << SEP << yvalues[j];
    groupReport << endl;

    for (unsigned int k = 0; k < group->children.size(); k++) {
        group->children[k].metaGroupId = group->metaGroupId;
        writeGroupInfo(&group->children[k]);
        //writePeakInfo(&group->children[k]);
    }

}

void CSVReports::writePeakInfo(PeakGroup* group) {
    if (!peakReport.is_open())
        return;
    string compoundName = "";
    string compoundID = "";

    if (group->compound != NULL) {
        // TODO: Added this while merging this file
        compoundName = sanitizeString(group->compound->name.c_str()).toStdString();
        compoundID   = sanitizeString(group->compound->id.c_str()).toStdString();
    }

    for (unsigned int j = 0; j < group->peaks.size(); j++) {
        Peak& peak = group->peaks[j];
        mzSample* sample = peak.getSample();
        string sampleName;
        if (sample != NULL)
            sampleName = sanitizeString(sample->sampleName.c_str()).toStdString();;

        peakReport << setprecision(8)
                << groupId << SEP
                << compoundName << SEP
                << compoundID << SEP
                << sampleName << SEP
                << peak.peakMz <<  SEP
                << peak.medianMz <<  SEP
                << peak.baseMz <<  SEP
                << setprecision(3)
                << peak.rt <<  SEP
                << peak.rtmin <<  SEP
                << peak.rtmax <<  SEP
                << peak.quality << SEP
                << peak.peakIntensity << SEP
                << peak.peakArea <<  SEP
                << peak.peakAreaTop <<  SEP
                << peak.peakAreaCorrected <<  SEP
                << peak.noNoiseObs <<  SEP
                << peak.signalBaselineRatio <<  SEP
                << peak.fromBlankSample << SEP
                << endl;
    }
}
