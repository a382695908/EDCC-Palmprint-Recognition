/*************************************************************************
    > File Name: palmprint_identify.cc
    > Author: Leosocy
    > Mail: 513887568@qq.com 
    > Created Time: 2017/07/26 21:27:26
 ************************************************************************/
#include <IO.h>
#include <Core.h>
#include <Check.h>
#include <EDCC.h>
using namespace EDCC;

size_t EncodeAllPalmprint(vector<PalmprintCode> &allPalmprint,
                          const map<string, int> &configMap);

size_t BuildUpAllFeaturesWhenIncremental(const vector<PalmprintCode> &originFeatures,
                                         const vector<PalmprintCode> &incrementalFeatures,
                                         vector<PalmprintCode> &allFeatures);

bool SortTopK(const PalmprintCode &onePalmrpint,
              const vector<PalmprintCode> &featuresAll,
              size_t k,
              map<size_t, MatchResult> &topKResult);

int EDCC::GetTrainingSetFeatures(const char *trainingSetPalmprintGroupFileName,
                                 const char *configFileName,
                                 const char *featuresOutputFileName,
                                 bool isIncremental)
{
    CHECK_POINTER_NULL_RETURN(trainingSetPalmprintGroupFileName, EDCC_NULL_POINTER_ERROR);
    CHECK_POINTER_NULL_RETURN(configFileName, EDCC_NULL_POINTER_ERROR);
    CHECK_POINTER_NULL_RETURN(featuresOutputFileName, EDCC_NULL_POINTER_ERROR);

    IO trainIO;
    vector<PalmprintCode> featuresAll;
    vector<PalmprintCode> featuresOrigin;
    Check checkHanler;
    int retCode = 0;

    if(!isIncremental) {
        ifstream configIn;
        configIn.open(configFileName);
        retCode = trainIO.loadConfig(configIn);
        CHECK_NE_RETURN(retCode, EDCC_SUCCESS, EDCC_LOAD_CONFIG_FAIL);
    } else {
        ifstream featuresIn;
        featuresIn.open(featuresOutputFileName);
        retCode = trainIO.loadPalmprintFeatureData(featuresIn, featuresOrigin);
        if(retCode != EDCC_SUCCESS
           || !checkHanler.checkPalmprintFeatureData(featuresOrigin, trainIO.configMap)) {
            return EDCC_LOAD_FEATURES_FAIL;
        }
    }
    if(!checkHanler.checkConfigValid(trainIO.configMap)) {
        return EDCC_LOAD_CONFIG_FAIL;
    }

    ifstream trainingSetIn;
    trainingSetIn.open(trainingSetPalmprintGroupFileName);
    retCode = trainIO.loadPalmprintGroup(trainingSetIn, featuresAll);
    if(retCode != EDCC_SUCCESS || !checkHanler.checkPalmprintGroupValid(featuresAll)) {
        return EDCC_LOAD_TAINING_SET_FAIL;
    }
    EncodeAllPalmprint(featuresAll, trainIO.configMap);
    if(isIncremental) {
        BuildUpAllFeaturesWhenIncremental(featuresOrigin, featuresAll, featuresAll);
    }

    ofstream featuresOutStream;
    featuresOutStream.open(featuresOutputFileName);
    retCode = trainIO.savePalmprintFeatureData(featuresOutStream, featuresAll);
    CHECK_NE_RETURN(retCode, EDCC_SUCCESS, EDCC_SAVE_FEATURES_FAIL);

    return EDCC_SUCCESS;
}

int EDCC::GetTwoPalmprintMatchScore(const char *firstPalmprintImagePath,
                                    const char *secondPalmprintImagePath,
                                    const char *configFileName,
                                    double &score)
{
    CHECK_POINTER_NULL_RETURN(firstPalmprintImagePath, EDCC_NULL_POINTER_ERROR);
    CHECK_POINTER_NULL_RETURN(secondPalmprintImagePath, EDCC_NULL_POINTER_ERROR);
    CHECK_POINTER_NULL_RETURN(configFileName, EDCC_NULL_POINTER_ERROR);

    IO matchIO;
    int retCode = 0;
    ifstream configIn;
    Check checkHanler;
    score = 0.0;

    configIn.open(configFileName);
    retCode = matchIO.loadConfig(configIn);
    CHECK_NE_RETURN(retCode, EDCC_SUCCESS, EDCC_LOAD_CONFIG_FAIL);
    if(!checkHanler.checkConfigValid(matchIO.configMap)) {
        return EDCC_LOAD_CONFIG_FAIL;
    }

    PalmprintCode firstPalmprint("identity", firstPalmprintImagePath);
    PalmprintCode secondPalmprint("identity", secondPalmprintImagePath);
    if(!firstPalmprint.encodePalmprint(matchIO.configMap)
       || !secondPalmprint.encodePalmprint(matchIO.configMap)) {
        return EDCC_PALMPRINT_IMAGE_NOT_EXISTS;
    }

    score = firstPalmprint.matchWith(secondPalmprint);

    return EDCC_SUCCESS;
}

int EDCC::GetTopKMatchScore(const char *onePalmprintImagePath,
                            const char *trainingSetFeaturesOrPalmprintGroupFileName,
                            const char *configFileName,
                            bool isFeatures,
                            size_t K,
                            map<size_t, MatchResult> &topKResult)
{
    CHECK_POINTER_NULL_RETURN(onePalmprintImagePath, EDCC_NULL_POINTER_ERROR);
    CHECK_POINTER_NULL_RETURN(trainingSetFeaturesOrPalmprintGroupFileName, EDCC_NULL_POINTER_ERROR);
    CHECK_POINTER_NULL_RETURN(configFileName, EDCC_NULL_POINTER_ERROR);

    IO matchIO;
    int retCode = 0;
    ifstream featuresOrGroupIn;
    vector<PalmprintCode> featuresAll;
    Check checkHanler;

    featuresOrGroupIn.open(trainingSetFeaturesOrPalmprintGroupFileName);
    if(isFeatures) {
        retCode = matchIO.loadPalmprintFeatureData(featuresOrGroupIn, featuresAll);
        if(!checkHanler.checkPalmprintFeatureData(featuresAll, matchIO.configMap)) {
            return EDCC_LOAD_FEATURES_FAIL;
        }
    } else {
        retCode = matchIO.loadPalmprintGroup(featuresOrGroupIn, featuresAll);
        if(retCode != EDCC_SUCCESS
           || !checkHanler.checkPalmprintGroupValid(featuresAll)) {
            return EDCC_LOAD_TAINING_SET_FAIL;
        }

        ifstream configIn;
        configIn.open(configFileName);
        retCode = matchIO.loadConfig(configIn);
    }
    if(retCode != EDCC_SUCCESS
       || !checkHanler.checkConfigValid(matchIO.configMap)) {
        return EDCC_LOAD_CONFIG_FAIL;
    }
    if(!isFeatures) {
        EncodeAllPalmprint(featuresAll, matchIO.configMap);
    }
    PalmprintCode onePalmprint("identity", onePalmprintImagePath);
    if(!onePalmprint.encodePalmprint(matchIO.configMap)) {
        return EDCC_PALMPRINT_IMAGE_NOT_EXISTS;
    }

    SortTopK(onePalmprint, featuresAll, K, topKResult);

    return EDCC_SUCCESS;
}

size_t EncodeAllPalmprint(vector<PalmprintCode> &allPalmprint,
                          const map< string, int > &configMap)
{
    vector<PalmprintCode>::iterator pcIt, pcItTmp;
    for(pcIt = allPalmprint.begin(); pcIt != allPalmprint.end();) {
        bool bRet;
        bRet = pcIt->encodePalmprint(configMap);
        if(!bRet) {
            pcItTmp = pcIt;
            pcIt = allPalmprint.erase(pcItTmp);
            continue;
        }
        ++pcIt;
    }

    return allPalmprint.size();
}

size_t BuildUpAllFeaturesWhenIncremental(const vector<PalmprintCode> &originFeatures,
                                         const vector<PalmprintCode> &incrementalFeatures,
                                         vector<PalmprintCode> &allFeatures)
{
    vector<PalmprintCode>::const_iterator pcIt, pcItTmp;
    allFeatures = incrementalFeatures;

    for(pcIt = originFeatures.begin(); pcIt != originFeatures.end(); ++pcIt) {
        bool isExists = false;
        for(pcItTmp = incrementalFeatures.begin();
            pcItTmp != incrementalFeatures.end();
            ++pcItTmp) {
            if(pcIt->imagePath == pcItTmp->imagePath
               && pcIt->identity == pcIt->identity) {
                cout << "--Cover\t" << pcIt->identity << " : " << pcIt->imagePath << endl;
                isExists = true;
                break;
            }
        }
        if(!isExists) {
            allFeatures.push_back(*pcIt);
        }
    }

    return allFeatures.size();
}

bool cmp(const MatchResult &result1, const MatchResult &result2)
{
    return result1.score > result2.score;
}

bool SortTopK(const PalmprintCode &onePalmrpint,
              const vector<PalmprintCode> &featuresAll,
              size_t k,
              map<size_t, MatchResult> &topKResult)
{
    vector<MatchResult> results;

    for(size_t i = 0; i < featuresAll.size(); ++i) {
        MatchResult oneResult;
        oneResult.identity = featuresAll.at(i).identity;
        oneResult.imagePath = featuresAll.at(i).imagePath;
        oneResult.score = featuresAll.at(i).matchWith(onePalmrpint);
        results.push_back(oneResult);
    }
    sort(results.begin(), results.end(), cmp);

    for(size_t i = 0; i < k && i < featuresAll.size(); ++i) {
        results.at(i).rank = i;
        topKResult.insert(map<size_t, MatchResult>::value_type(i, results.at(i)));
    }

    return true;
}
