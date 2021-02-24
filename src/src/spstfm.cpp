// #include "spstfm.h"
// #include <Rcpp.h>
// 
// namespace imagefusion {
// 
// using namespace spstfm_impl_detail;
// 
// void SpstfmFusor::processOptions(Options const& o) {
//     options_type newOpts = dynamic_cast<options_type const&>(o);
// 
//     // check that the options are not illegal
//     if (newOpts.dictSize > newOpts.numberTrainingSamples)
//         IF_THROW_EXCEPTION(invalid_argument_error("You have set the dictionary size to " + std::to_string(newOpts.dictSize) +
//                                                   ", which is larger than the training size, which you set to " + std::to_string(newOpts.numberTrainingSamples)));
// 
//     if (newOpts.patchOverlap > newOpts.patchSize / 2)
//         IF_THROW_EXCEPTION(invalid_argument_error("You have set the patch overlap (per side) to " + std::to_string(newOpts.patchOverlap) +
//                                                   ", which is larger than half of the patch size, which you set to " + std::to_string(newOpts.patchSize) + " x "  + std::to_string(newOpts.patchSize) +
//                                                   ". This would result in an overlap by the second neighbour, which is not allowed (and not implemented)."));
// 
//     if (!newOpts.isDate1Set)
//         IF_THROW_EXCEPTION(invalid_argument_error("You have not set the date of the first input pair (date1)."));
// 
//     if (!newOpts.isDate3Set)
//         IF_THROW_EXCEPTION(invalid_argument_error("You have not set the date of the second input pair (date3)."));
// 
//     if (newOpts.date1 == newOpts.date3)
//         IF_THROW_EXCEPTION(invalid_argument_error("The dates for the input pairs have to be different. You chose " + std::to_string(newOpts.date1) + " for both."));
// 
//     if (newOpts.highTag == newOpts.lowTag)
//         IF_THROW_EXCEPTION(invalid_argument_error("The resolution tags for the input pairs have to be different. You chose '" + newOpts.highTag + "' for both."));
// 
//     if (newOpts.minTrainIter > newOpts.maxTrainIter)
//         IF_THROW_EXCEPTION(invalid_argument_error("The minimum number of training iterations must not be larger than the maximum number of training iterations. You chose min: '" + std::to_string(newOpts.minTrainIter) + "' and max: '" + std::to_string(newOpts.maxTrainIter) + "."));
// 
//     if (newOpts.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::test_set_error && newOpts.getTrainingStopNumberTestSamples() == 0)
//         IF_THROW_EXCEPTION(invalid_argument_error("When using the test_set_error as TrainingStopFunction, the number of test samples may not be zero."));
// 
//     if (newOpts.dbg_recordTrainingStopFunctions && newOpts.getTrainingStopNumberTestSamples() == 0)
//         IF_THROW_EXCEPTION(invalid_argument_error("When recording all stopping functions, test_set_error is also used. But then the number of test samples may not be zero."));
// 
//     if (newOpts.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::train_set && newOpts.getSparseCoeffTrainingResolution() != SpstfmOptions::TrainingResolution::low)
//         IF_THROW_EXCEPTION(invalid_argument_error("When using best shot dictionary with train set to determine the error, the sparse coefficient algorithm (GPSR) in training has to use the low resolution. If you want another resolution, but still want to use the best shot dictionary state, use the test set with a non-zero number of test samples."));
// 
//     if (newOpts.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::test_set && newOpts.getTrainingStopNumberTestSamples() == 0)
//         IF_THROW_EXCEPTION(invalid_argument_error("When using best shot dictionary with the test set to determine the error, the number of test samples may not be zero."));
// 
//     opt = newOpts;
// }
// 
// 
// void SpstfmFusor::checkInputImages(ConstImage const& validMask, ConstImage const& predMask, int date2, bool useDate2) const {
//     if (!imgs)
//         IF_THROW_EXCEPTION(logic_error("No MultiResImage object stored in SpstfmFusor while predicting. This looks like a programming error."));
// 
//     std::string strH1 = "High resolution image (tag: " + opt.getHighResTag() + ") at date 1 (date: " + std::to_string(opt.getDate1()) + ")";
//     std::string strH3 = "High resolution image (tag: " + opt.getHighResTag() + ") at date 3 (date: " + std::to_string(opt.getDate3()) + ")";
//     std::string strL1 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 1 (date: " + std::to_string(opt.getDate1()) + ")";
//     std::string strL2 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 2 (date: " + std::to_string(date2)     + ")";
//     std::string strL3 = "Low resolution image (tag: "  + opt.getLowResTag()  + ") at date 3 (date: " + std::to_string(opt.getDate3()) + ")";
// 
//     if (!imgs->has(opt.getLowResTag(),  opt.getDate1()) || !imgs->has(opt.getLowResTag(),  opt.getDate3()) || (useDate2 && !imgs->has(opt.getLowResTag(), date2)) ||
//         !imgs->has(opt.getHighResTag(), opt.getDate1()) || !imgs->has(opt.getHighResTag(), opt.getDate3()))
//     {
//         IF_THROW_EXCEPTION(not_found_error("Not all required images are available. For SPSTFM you need to provide:\n"
//                                            " * " + strH1 + " [" + (imgs->has(opt.getHighResTag(), opt.getDate1()) ? "" : "NOT ") + "available]\n"
//                                            " * " + strH3 + " [" + (imgs->has(opt.getHighResTag(), opt.getDate3()) ? "" : "NOT ") + "available]\n"
//                                            " * " + strL1 + " [" + (imgs->has(opt.getLowResTag(),  opt.getDate1()) ? "" : "NOT ") + "available]\n" +
//                                (useDate2 ? " * " + strL2 + " [" + (imgs->has(opt.getLowResTag(),  date2)     ? "" : "NOT ") + "available]\n" : "") +
//                                            " * " + strL3 + " [" + (imgs->has(opt.getLowResTag(),  opt.getDate3()) ? "" : "NOT ") + "available]"));
//     }
// 
//     Type highType = imgs->get(opt.getHighResTag(), opt.getDate3()).type();
//     if (imgs->get(opt.getHighResTag(), opt.getDate1()).type() != highType)
//         IF_THROW_EXCEPTION(image_type_error("The data types for the high resolution images are different:\n"
//                                             " * " + strH1 + ": " + to_string(imgs->get(opt.getHighResTag(), opt.getDate1()).type()) + " and\n"
//                                             " * " + strH3 + ": " + to_string(imgs->get(opt.getHighResTag(), opt.getDate3()).type())));
// 
//     Type lowType  = imgs->get(opt.getLowResTag(), opt.getDate3()).type();
//     if (imgs->get(opt.getLowResTag(), opt.getDate1()).type() != lowType || (useDate2 && imgs->get(opt.getLowResTag(), date2).type() != lowType))
//         IF_THROW_EXCEPTION(image_type_error("The data types for the low resolution images are different:\n"
//                                             " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(), opt.getDate1()).type()) + ",\n" +
//                                 (useDate2 ? " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(), date2).type())     + " and\n" : "") +
//                                             " * " + strL3 + " " + to_string(imgs->get(opt.getLowResTag(), opt.getDate3()).type())));
// 
//     Size s = imgs->get(opt.getLowResTag(), opt.getDate3()).size();
//     if (imgs->get(opt.getHighResTag(),  opt.getDate1()).size() != s ||
//         imgs->get(opt.getHighResTag(),  opt.getDate3()).size() != s ||
//         imgs->get(opt.getLowResTag(), opt.getDate1()).size() != s ||
//         (useDate2 && imgs->get(opt.getLowResTag(), date2).size() != s))
//     {
//         IF_THROW_EXCEPTION(size_error("The required images have a different size:\n"
//                                       " * " + strH1 + " " + to_string(imgs->get(opt.getHighResTag(), opt.getDate1()).size()) + "\n"
//                                       " * " + strH3 + " " + to_string(imgs->get(opt.getHighResTag(), opt.getDate3()).size()) + "\n"
//                                       " * " + strL1 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.getDate1()).size()) + "\n" +
//                           (useDate2 ? " * " + strL2 + " " + to_string(imgs->get(opt.getLowResTag(),  date2).size())     + "\n" : "") +
//                                       " * " + strL3 + " " + to_string(imgs->get(opt.getLowResTag(),  opt.getDate3()).size())));
//     }
// 
//     if (!validMask.empty() && validMask.size() != s)
//         IF_THROW_EXCEPTION(size_error("The validMask has a wrong size: " + to_string(validMask.size()) +
//                                       ". It must have the same size as the images: " + to_string(s) + "."))
//                 << errinfo_size(validMask.size());
// 
//     if (!validMask.empty() && validMask.basetype() != Type::uint8)
//         IF_THROW_EXCEPTION(image_type_error("The validMask has a wrong base type: " + to_string(validMask.basetype()) +
//                                       ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
//                 << errinfo_image_type(validMask.basetype());
// 
//     if (getChannels(lowType) != getChannels(highType))
//         IF_THROW_EXCEPTION(image_type_error("The number of channels of the low resolution images (" + std::to_string(getChannels(lowType)) +
//                                             ") are different than of the high resolution images (" + std::to_string(getChannels(highType)) + ")."));
// 
//     if (!validMask.empty() && validMask.channels() != 1 && validMask.channels() != getChannels(lowType))
//         IF_THROW_EXCEPTION(image_type_error("The validMask has a wrong number of channels. It has " + std::to_string(validMask.channels()) + " channels while the images have "
//                                             + std::to_string(getChannels(lowType)) + ". The mask should have either 1 channel or the same number of channels as the images."))
//                 << errinfo_image_type(validMask.type());
// 
//     if (!predMask.empty() && predMask.size() != s)
//         IF_THROW_EXCEPTION(size_error("The predMask has a wrong size: " + to_string(predMask.size()) +
//                                       ". It must have the same size as the images: " + to_string(s) + "."))
//                 << errinfo_size(predMask.size());
// 
//     if (!predMask.empty() && predMask.basetype() != Type::uint8)
//         IF_THROW_EXCEPTION(image_type_error("The predMask has a wrong base type: " + to_string(predMask.basetype()) +
//                                       ". To represent boolean values with 0 or 255, it must have the basetype: " + to_string(Type::uint8) + "."))
//                 << errinfo_image_type(predMask.basetype());
// 
//     if (!predMask.empty() && predMask.channels() != 1)
//         IF_THROW_EXCEPTION(image_type_error("The predMask must be a single-channel mask, but it has "
//                                             + std::to_string(predMask.channels()) + " channels."))
//                 << errinfo_image_type(predMask.type());
// 
//     if (opt.useBuildUpIndexForWeights() && getChannels(lowType) < 3)
//         IF_THROW_EXCEPTION(image_type_error("The number of channels of the input images is less than 3 (it is " + std::to_string(getChannels(lowType)) +
//                                             "), but you selected to use the build-up index for weighting, which requires red, NIR and SWIR channels")) << errinfo_image_type(lowType);
// 
//     if (opt.useBuildUpIndexForWeights())
//         for (unsigned int channel : opt.red_nir_swir_order)
//             if (channel >= getChannels(lowType))
//                 IF_THROW_EXCEPTION(image_type_error("You selected to use the build-up index for weighting and specified the order of red, NIR and SWIR channels. "
//                                                     "You specified to use channel " + std::to_string(channel) + " for that, but the images have only "
//                                                     + std::to_string(getChannels(lowType)) + " channels.")) << errinfo_image_type(lowType);
// }
// 
// 
// void SpstfmFusor::train(ConstImage const& validMask, ConstImage const& predMask) {
//     // check that all required images are available and have the same type and size
//     checkInputImages(validMask, predMask);
//     Size s        = imgs->get(opt.getLowResTag(), opt.getDate3()).size();
//     Type lowType  = imgs->get(opt.getLowResTag(), opt.getDate3()).type();
//     Type highType = imgs->get(opt.getHighResTag(), opt.getDate3()).type();
// 
//     // if no prediction area has been set, use full img size
//     Rectangle predArea = opt.getPredictionArea();
//     if (predArea.x == 0 && predArea.y == 0 && predArea.width == 0 && predArea.height == 0) {
//         predArea.width  = imgs->getAny().width();
//         predArea.height = imgs->getAny().height();
//     }
// 
//     // get the sample area, i. e. the prediction area and a bit around to have full patches (may be out of bounds)
//     Rectangle sampleArea = calcRequiredArea(predArea, opt.patchSize, opt.patchOverlap);
// 
//     // make sample area without going out of bounds
//     Rectangle innerSampleArea = sampleArea & Rectangle(0, 0, s.width, s.height);
// 
// //    Rcpp::Rcout << "crop: " << crop << std::endl
// //              << "predArea: " << predArea << std::endl;
// 
//     ConstImage const& h1 = imgs->get(opt.getHighResTag(), opt.getDate1());
//     ConstImage const& h3 = imgs->get(opt.getHighResTag(), opt.getDate3());
//     ConstImage const& l1 = imgs->get(opt.getLowResTag(),  opt.getDate1());
//     ConstImage const& l3 = imgs->get(opt.getLowResTag(),  opt.getDate3());
// 
//     // limit validMask to sample area
//     t.sampleMask = Image{s, getFullType(Type::uint8, validMask.channels())};
//     t.sampleMask.set(0);
//     t.sampleMask.crop(innerSampleArea);
//     if (!validMask.empty()) {
//         ConstImage innerMask = validMask.sharedCopy(innerSampleArea);
//         t.sampleMask.set(255, innerMask);
//     }
//     else
//         t.sampleMask.set(255);
//     t.sampleMask.uncrop();
// 
//     t.writeMask = predMask.constSharedCopy();
// 
// //    {
// //        Rcpp::Rcout << "x = xout + " << crop.x << std::endl;
// //        Rcpp::Rcout << "pxi = floor(x / (" << opt.patchSize << " - " << opt.patchOverlap << "))" << std::endl;
// //        Rcpp::Rcout << "xpatch = x - pxi * (" << opt.patchSize << " - " << opt.patchOverlap << ")" << std::endl;
// 
// //        Rcpp::Rcout << "y = yout + " << crop.y << std::endl;
// //        Rcpp::Rcout << "pyi = floor(y / (" << opt.patchSize << " - " << opt.patchOverlap << "))" << std::endl;
// //        Rcpp::Rcout << "ypatch = y - pyi * (" << opt.patchSize << " - " << opt.patchOverlap << ")" << std::endl;
// //    }
// 
//     Type resultHighType = getResultType(highType);
//     Type resultLowType  = getResultType(lowType);
//     Image highDiff = h3.subtract(h1, resultHighType);
//     Image lowDiff  = l3.subtract(l1, resultLowType);
// 
//     // calculate sample norm factor and mean. (Used by DictTrainer::train(), DictTrainer::getSamples() and DictTrainer::reconstructImage(). Thus SpstfmFusor::train() must be called within SpstfmFusor::predict() to set these values!)
//     unsigned int chans = h1.channels();
//     auto meanStdDevHigh = highDiff.meanStdDev(t.sampleMask);
//     auto meanStdDevLow  = lowDiff.meanStdDev(t.sampleMask);
//     t.meansOfHighDiff     = std::move(meanStdDevHigh.first);
//     t.normFactorsHighDiff = std::move(meanStdDevHigh.second);
//     t.meansOfLowDiff      = std::move(meanStdDevLow.first);
//     t.normFactorsLowDiff  = std::move(meanStdDevLow.second);
// 
//     if (!opt.useStdDevForSampleNormalization()) {
//         // ok, use variance (square elements)
//         for (double& f : t.normFactorsHighDiff)
//             f *= f;
//         for (double& f : t.normFactorsLowDiff)
//             f *= f;
//     }
// 
//     t.meanForHighDiff_cv = t.meansOfHighDiff;
//     t.meanForLowDiff_cv  = t.meansOfLowDiff;
//     if (opt.getSubtractMeanUsage() == SpstfmOptions::SampleNormalization::low)
//         t.meanForHighDiff_cv = t.meanForLowDiff_cv;
//     else if (opt.getSubtractMeanUsage() == SpstfmOptions::SampleNormalization::high)
//         t.meanForLowDiff_cv = t.meanForHighDiff_cv;
//     else if (opt.getSubtractMeanUsage() == SpstfmOptions::SampleNormalization::none)
//         t.meanForLowDiff_cv = t.meanForHighDiff_cv = std::vector<double>(chans, 0.0);
//     // else SampleNormalization::separate, change nothing
// 
//     if (opt.getDivideNormalizationFactor() == SpstfmOptions::SampleNormalization::low)
//         t.normFactorsHighDiff = t.normFactorsLowDiff;
//     else if (opt.getDivideNormalizationFactor() == SpstfmOptions::SampleNormalization::high)
//         t.normFactorsLowDiff = t.normFactorsHighDiff;
//     else if (opt.getDivideNormalizationFactor() == SpstfmOptions::SampleNormalization::none)
//         t.normFactorsLowDiff = t.normFactorsHighDiff = std::vector<double>(chans, 1.0);
//     // else SampleNormalization::separate, change nothing
// 
//     // init dictionary memory vector
//     t.opt = opt;
//     if (t.dictsConcat.size() != chans) {
//         t.dictsConcat.clear();
//         t.dictsConcat.resize(chans);
// //        t.trainedAtoms.clear();
// //        t.trainedAtoms.resize(chans);
//     }
// 
//     // train for each channel separately
//     unsigned int dim = opt.getPatchSize() * opt.getPatchSize();
//     for (unsigned int c = 0; c < chans; ++c) {
//         if (t.dictsConcat[c].n_rows != 2 * dim || t.dictsConcat[c].n_cols == 0 ||
//                 opt.getDictionaryReuse() != SpstfmOptions::ExistingDictionaryHandling::use)
//         {
//             auto samplePair = t.getSamples(highDiff, lowDiff, sampleArea, c); // uses sampleMask and mean and stddev from above
//             arma::mat trainingSamplesConcat   = std::move(samplePair.first);
//             arma::mat validationSamplesConcat = std::move(samplePair.second);
// //            Rcpp::Rcout << "Samples (c = " << c << "):" << std::endl << trainingSamplesConcat << std::endl;
// 
//             t.initDictsFromSamples(trainingSamplesConcat, c);
// 
//             t.train(trainingSamplesConcat, validationSamplesConcat, c);
//         }
//     }
// }
// 
// 
// void SpstfmFusor::predict(int date2, ConstImage const& validMask, ConstImage const& predMask) {
//     checkInputImages(validMask, predMask, date2, true);
// 
//     // if no prediction area has been set, use full img size
//     Rectangle predArea = opt.getPredictionArea();
//     if (predArea.x == 0 && predArea.y == 0 && predArea.width == 0 && predArea.height == 0) {
//         predArea.width  = imgs->getAny().width();
//         predArea.height = imgs->getAny().height();
//     }
// 
//     // maybe create output buffer
//     Type highType = imgs->get(opt.getHighResTag(), opt.getDate3()).type();
//     if (output.size() != predArea.size() || output.type() != highType)
//         output = Image{predArea.size(), highType}; // create a new one
//     t.output = Image{output.sharedCopy()};
// 
//     // get the sample area, i. e. the prediction area and a bit around to have full patches (may be out of bounds)
//     Rectangle sampleArea = calcRequiredArea(predArea, opt.patchSize, opt.patchOverlap);
// 
//     // train dictionaries
//     train(validMask, predMask); // also calculates mean values (used in reconstructImage()) and sampleMask (used in initWeights())
// 
//     ConstImage const& h1 = imgs->get(opt.getHighResTag(), opt.getDate1());
//     ConstImage const& h3 = imgs->get(opt.getHighResTag(), opt.getDate3());
//     ConstImage const& l1 = imgs->get(opt.getLowResTag(),  opt.getDate1());
//     ConstImage const& l2 = imgs->get(opt.getLowResTag(),      date2);
//     ConstImage const& l3 = imgs->get(opt.getLowResTag(),  opt.getDate3());
// 
//     // init weights (multi-channel & build-up index)
//     if (opt.useBuildUpIndexForWeights()) {
//         auto const& order = opt.getRedNirSwirOrder();
//         t.initWeights(h1, h3, l1, l2, l3, sampleArea, {order.begin(), order.end()}); // uses sampleMask
//     }
// 
//     // calculate fill values for reconstruction
//     std::vector<double> meanL1 = l1.mean(t.sampleMask);
//     std::vector<double> meanL2 = l2.mean(t.sampleMask);
//     std::vector<double> meanL3 = l3.mean(t.sampleMask);
// 
//     unsigned int chans = h1.channels();
//     for (unsigned int c = 0; c < chans; ++c) {
//         // init weights (single-channel | no build-up index)
//         if (!opt.useBuildUpIndexForWeights())
//             t.initWeights(h1, h3, l1, l2, l3, sampleArea, {c});
// 
//         double fillL21 = meanL2[c] - meanL1[c];
//         double fillL23 = meanL2[c] - meanL3[c];
//         t.reconstructImage(h1, h3, l1, l2, l3, fillL21, fillL23, predArea, sampleArea, c);
//     }
// }
// 
// std::pair<arma::mat, arma::mat> spstfm_impl_detail::DictTrainer::getSamples(ConstImage const& highDiff, ConstImage const& lowDiff, imagefusion::Rectangle sampleArea, unsigned int channel) const {
//     double meanForHigh       = meanForHighDiff_cv[channel];
//     double meanForLow        = meanForLowDiff_cv[channel];
//     double normFactorForHigh = normFactorsHighDiff[channel];
//     double normFactorForLow  = normFactorsLowDiff[channel];
//     double fillHigh          = meansOfHighDiff[channel];
//     double fillLow           = meansOfLowDiff[channel];
// //        Rcpp::Rcout << "Using norm factor: " << normFactorForHigh << " for high res and " << normFactorForLow << " for low res." << std::endl;
// 
//     // determine the order of patches and the number to sample and to use for dictionary (limited, if the image is too small)
//     std::vector<size_t> patchIndices = getOrderedPatchIndices(opt.getSamplingStrategy(), highDiff, lowDiff, sampleMask, opt.getInvalidPixelTolerance(), opt.getPatchSize(), opt.getPatchOverlap(), sampleArea, channel);
//     unsigned int nsamples = opt.getNumberTrainingSamples();
//     if (nsamples > patchIndices.size()) {
//         nsamples = patchIndices.size();
// 
//         double reduction = static_cast<double>(nsamples) / opt.getNumberTrainingSamples();
//         unsigned int natoms = static_cast<unsigned int>(opt.getDictSize() * reduction);
//         Rcpp::Rcout << "Changed nsamples to " << nsamples << " and natoms to " << natoms << ".";
//         if (natoms <= opt.getPatchSize() * opt.getPatchSize())
//             Rcpp::Rcout << " Note, the dictionary is not overcomplete, since it only has " << natoms
//                       << " atoms with " << (opt.getPatchSize() * opt.getPatchSize()) << " elements each.";
//         Rcpp::Rcout << std::endl;
//     }
// 
//     arma::mat validationSamplesConcat;
//     if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::test_set_error || opt.dbg_recordTrainingStopFunctions || opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::test_set) {
//         unsigned int nValSamples = std::min(opt.getTrainingStopNumberTestSamples(), (unsigned int)(patchIndices.size()));
//         unsigned int dist = opt.getPatchSize() - opt.getPatchOverlap();
//         unsigned int npx = (sampleArea.width  - opt.getPatchOverlap()) / dist;
//         unsigned int npy = (sampleArea.height - opt.getPatchOverlap()) / dist;
// 
//         std::vector<size_t> valPatchIndices = uniqueRandomVector(npx * npy);
//         valPatchIndices.resize(nValSamples);
// 
//         std::vector<size_t> invalid = mostlyInvalidPatches(sampleMask, opt.getInvalidPixelTolerance(), opt.getPatchSize(), opt.getPatchOverlap(), sampleArea, channel);
//         for (size_t pi : invalid)
//             valPatchIndices.erase(std::remove(valPatchIndices.begin(), valPatchIndices.end(), pi), valPatchIndices.end());
// 
//         validationSamplesConcat = samples(highDiff, lowDiff, sampleMask, valPatchIndices, meanForHigh, meanForLow, normFactorForHigh, normFactorForLow, fillHigh, fillLow, opt.getPatchSize(), opt.getPatchOverlap(), sampleArea, channel);
//     }
// 
// //    /// DEBUG
// //    std::vector<size_t> unusedPatchIndices(patchIndices.begin() + nsamples, patchIndices.end());
// //    arma::mat unusedSamplesConcat = samples(highDiff, lowDiff, unusedPatchIndices, mean, stddev, opt.getPatchSize(), opt.getPatchOverlap(), channel);
// //    drawDictionary(unusedSamplesConcat, "unused_patches.png");
// 
//     patchIndices.resize(nsamples);
//     arma::mat trainingSamplesConcat = samples(highDiff, lowDiff, sampleMask, patchIndices, meanForHigh, meanForLow, normFactorForHigh, normFactorForLow, fillHigh, fillLow, opt.getPatchSize(), opt.getPatchOverlap(), sampleArea, channel);
// //    drawDictionary(trainingSamplesConcat, "training_patches.png");     /// DEBUG
// 
//     return std::make_pair(std::move(trainingSamplesConcat), std::move(validationSamplesConcat));
// }
// 
// void spstfm_impl_detail::DictTrainer::initWeights(ConstImage const& /*high1*/, ConstImage const& /*high3*/, ConstImage const& low1, ConstImage const& low2, ConstImage const& low3, Rectangle sampleArea, std::vector<unsigned int> const& channels) {
//     cv::Mat d12, d32;
//     ConstImage singleChannelMask;
//     double scale;
//     if (channels.size() == 1) {
//         unsigned int channel = channels.front();
//         Image l1c1, l2c1, l3c1;
//         if (low1.channels() > 1)
//             l1c1 = std::move(low1.split({channel}).front());
//         if (low2.channels() > 1)
//             l2c1 = std::move(low2.split({channel}).front());
//         if (low3.channels() > 1)
//             l3c1 = std::move(low3.split({channel}).front());
//         if (sampleMask.channels() > 1)
//             singleChannelMask = std::move(sampleMask.split({channel}).front());
//         else
//             singleChannelMask = sampleMask.constSharedCopy();
// 
//         ConstImage const& l1 = low1.channels() == 1 ? low1 : l1c1;
//         ConstImage const& l2 = low2.channels() == 1 ? low2 : l2c1;
//         ConstImage const& l3 = low3.channels() == 1 ? low3 : l3c1;
// 
//         Image diff;
//         diff = l1.subtract(l2, getResultType(l2.type())).abs();
//         d12 = std::move(diff.cvMat());
// 
//         diff = l3.subtract(l2, getResultType(l2.type())).abs();
//         d32 = std::move(diff.cvMat());
// //        cv::absdiff(low1.cvMat(), low2.cvMat(), d12);
// //        cv::absdiff(low3.cvMat(), low2.cvMat(), d32);
// 
//         double max12, max32;
//         cv::minMaxIdx(d12, nullptr, &max12, nullptr, nullptr, singleChannelMask.cvMat());
//         cv::minMaxIdx(d32, nullptr, &max32, nullptr, nullptr, singleChannelMask.cvMat());
//         scale = 1 / std::max(max12, max32);
//     }
//     else {
//         assert(channels.size() == 3);
// 
//         // combine all channels of the mask to one (with AND)
//         if (sampleMask.channels() > 1) {
//             auto masks = sampleMask.split();
//             for (auto& m : masks)
//                 if (singleChannelMask.empty())
//                     singleChannelMask = std::move(m);
//                 else
//                     singleChannelMask = singleChannelMask.bitwise_and(std::move(m));
//         }
//         else
//             singleChannelMask = sampleMask.constSharedCopy();
// 
//         // get continuous build-up index for low resolution images
//         Image bu1 = low1.convertColor(ColorMapping::Red_NIR_SWIR_to_BU, Type::int8, channels);
//         Image bu2 = low2.convertColor(ColorMapping::Red_NIR_SWIR_to_BU, Type::int8, channels);
//         Image bu3 = low3.convertColor(ColorMapping::Red_NIR_SWIR_to_BU, Type::int8, channels);
// //        Rcpp::Rcout << "bu1:" << std::endl << bu1.cvMat() << std::endl;
// //        Rcpp::Rcout << "bu2:" << std::endl << bu2.cvMat() << std::endl;
// //        Rcpp::Rcout << "bu3:" << std::endl << bu3.cvMat() << std::endl;
// 
//         // threshold the images to binary build-up mask
//         int8_t thresh = cv::saturate_cast<int8_t>(opt.getBUThreshold() * getImageRangeMax(Type::int8));
//         cv::Mat temp;
// 
//         // make mask images (uint8, with 0 or 255)
//         temp = bu1.cvMat() > thresh;
//         bu1.cvMat() = std::move(temp);
// 
//         temp = bu2.cvMat() > thresh;
//         bu2.cvMat() = std::move(temp);
// 
//         temp = bu3.cvMat() > thresh;
//         bu3.cvMat() = std::move(temp);
// //        Rcpp::Rcout << "bu1 binary:" << std::endl << bu1.cvMat() << std::endl;
// //        Rcpp::Rcout << "bu2 binary:" << std::endl << bu2.cvMat() << std::endl;
// //        Rcpp::Rcout << "bu3 binary:" << std::endl << bu3.cvMat() << std::endl;
// 
//         // get pixel changes (0 or 255)
//         cv::absdiff(bu1.cvMat(), bu2.cvMat(), d12);
//         cv::absdiff(bu3.cvMat(), bu2.cvMat(), d32);
// //        Rcpp::Rcout << "diff12 binary:" << std::endl << d12 << std::endl;
// //        Rcpp::Rcout << "diff32 binary:" << std::endl << d32 << std::endl;
//         scale = 1. / 255.;
//     }
// 
// 
//     // for every patch
//     unsigned int psize = opt.getPatchSize();
//     unsigned int pover = opt.getPatchOverlap();
//     unsigned int dist = psize - pover;
//     unsigned int dim = psize * psize;
//     unsigned int npx = (sampleArea.width  - pover) / dist;
//     unsigned int npy = (sampleArea.height - pover) / dist;
//     weights1.set_size(npy, npx);
//     weights3.set_size(npy, npx);
//     arma::mat p12, p32;
//     for (unsigned int pyi = 0; pyi < npy; ++pyi) {
//         for (unsigned int pxi = 0; pxi < npx; ++pxi) {
//             p12 = extractPatch(d12, pxi, pyi, psize, pover, sampleArea, /*channel*/ 0);
//             p32 = extractPatch(d32, pxi, pyi, psize, pover, sampleArea, /*channel*/ 0);
// 
//             auto maskPatch = extractPatch(singleChannelMask, pxi, pyi, psize, pover, sampleArea, /*channel*/ 0);
//             arma::uvec invalidIndices = arma::find(maskPatch == 0);
//             p12.elem(invalidIndices).fill(0);
//             p32.elem(invalidIndices).fill(0);
//             unsigned int nInval = invalidIndices.n_elem;
//             if (nInval == dim)
//                 --invalidIndices;
// 
//             // detemine the average number of changed pixels per pixel
//             double v1 = arma::accu(p12) * scale / (dim - nInval);
//             double v3 = arma::accu(p32) * scale / (dim - nInval);
// 
//             // calculate weights in [0,1] with w1 + w3 == 1 and catch inf and NaN
//             double w1 = 1/v1 / (1/v1 + 1/v3);
//             double w3 = 1/v3 / (1/v1 + 1/v3);
//             if (v1 == 0 && v3 == 0)
//                 w1 = w3 = 0.5;
//             else if (std::abs(v1 - v3) > opt.getWeightsDiffTol() || v1 == 0 || v3 == 0) {
//                 w1 = v1 < v3 ? 1 : 0;
//                 w3 = 1 - w1;
//             }
// 
//             weights1(pyi, pxi) = w1;
//             weights3(pyi, pxi) = w3;
//         }
//     }
// //    drawWeights(weights1, "weights.png");
// //    Rcpp::Rcout << "Number of non-zero weights: "
// //              << (arma::accu(weights1 != 0) + arma::accu(weights3 != 0)) << std::endl;
// }
// 
// void spstfm_impl_detail::DictTrainer::initDictsFromSamples(arma::mat const& samplesConcat, unsigned int channel) {
//     SpstfmOptions::ExistingDictionaryHandling reuse = opt.getDictionaryReuse();
//     SpstfmOptions::DictionaryNormalization normalization = opt.getDictionaryInitNormalization();
//     double reduction = static_cast<double>(samplesConcat.n_cols) / opt.getNumberTrainingSamples();
//     unsigned int natoms = static_cast<unsigned int>(opt.getDictSize() * reduction);
//     unsigned int doubledim = samplesConcat.n_rows;
//     arma::mat& dictConcat = dictsConcat[channel];
//     if (dictConcat.n_rows != doubledim || dictConcat.n_cols == 0 ||
//             reuse == SpstfmOptions::ExistingDictionaryHandling::clear)
//     {
//         // use first (best) samples to initialize dictionary
//         dictConcat.set_size(doubledim, natoms);
//         dictConcat = samplesConcat.head_cols(natoms);
// //        trainedAtoms[channel] = 0;
// 
//         // normalise them
//         using Norm = SpstfmOptions::DictionaryNormalization;
//         if (normalization == Norm::fixed) {
//             double scale = arma::norm(highMatView(dictConcat).col(0));
//             dictConcat *= 1 / scale;
//         }
//         else if (normalization == Norm::independent) {
//             arma::subview<double> dictHigh = highMatView(dictConcat);
//             arma::subview<double> dictLow  = lowMatView(dictConcat);
//             dictHigh = normalise(dictHigh);
//             dictLow  = normalise(dictLow);
//         }
//         else if (normalization == Norm::pairwise) {
//             for (unsigned int i = 0; i < natoms; ++i) {
//                 double nh = arma::norm(highMatView(dictConcat).col(i));
//                 double nl = arma::norm(lowMatView(dictConcat).col(i));
//                 highMatView(dictConcat).col(i) /= std::max(nh, nl);
//                 lowMatView(dictConcat).col(i)  /= std::max(nh, nl);
//             }
//         }
//         // otherwise Norm::none is selected, which should do nothing
//     }
//     else { // if (reuse == SpstfmOptions::ExistingDictionaryHandling::improve && size ok)
//         // only add atoms if there are desired more (do not remove atoms)
//         int nMissing = natoms - dictConcat.n_cols;
//         if (nMissing <= 0)
//             return;
// 
//         // select random samples
//         auto patchIndices = uniqueRandomVector(samplesConcat.n_cols);
//         patchIndices.resize(nMissing);
// 
//         // add them
//         dictConcat.resize(doubledim, natoms);
//         for (int k = 0; k < nMissing; ++k) {
//             unsigned int pi = patchIndices[k];
//             dictConcat.col(natoms - 1 - k) = samplesConcat.col(pi);
//         }
// //        trainedAtoms[channel] = natoms - nMissing;
// 
//         // normalise them
//         using Norm = SpstfmOptions::DictionaryNormalization;
//         arma::subview<double> newAtoms = dictConcat.tail_cols(nMissing);
//         if (normalization == Norm::fixed) {
//             double scale = arma::norm(highMatView(newAtoms).col(0));
//             newAtoms *= 1 / scale;
//         }
//         else if (normalization == Norm::independent) {
//             arma::subview<double> newHigh = highMatView(newAtoms);
//             arma::subview<double> newLow  = lowMatView(newAtoms);
//             newHigh = normalise(newHigh);
//             newLow  = normalise(newLow);
//         }
//         else if (normalization == Norm::pairwise) {
//             for (int i = 0; i < nMissing; ++i) {
//                 double nh = arma::norm(highMatView(newAtoms).col(i));
//                 double nl = arma::norm(lowMatView(newAtoms).col(i));
//                 highMatView(newAtoms).col(i) /= std::max(nh, nl);
//                 lowMatView(newAtoms).col(i)  /= std::max(nh, nl);
//             }
//         }
//         // otherwise Norm::none is selected, which should do nothing
//     }
// }
// 
// 
// void spstfm_impl_detail::DictTrainer::train(arma::mat& trainingSamplesConcat, arma::mat& validationSamplesConcat, unsigned int channel) {
//     // no training wanted?
//     if (opt.getMaxTrainIter() == 0)
//         return;
// 
//     double normFactorForHigh = normFactorsHighDiff[channel];
// 
//     arma::mat& dictConcat = dictsConcat[channel];
// 
//     using namespace std::chrono;
//     unsigned int nsamples = trainingSamplesConcat.n_cols;
//     unsigned int natoms   = dictConcat.n_cols;
//     if (nsamples == 0 || natoms == 0)
//         // no valid location
//         return;
// 
//     unsigned int psize = opt.getPatchSize();
//     unsigned int dim = psize * psize;
//     assert(dictConcat.n_rows  == 2 * dim && "dictConcat must be initialized before training (wrong number of rows)");
// 
//     assert(trainingSamplesConcat.n_rows  == 2 * dim && "trainingSamplesConcat must be initialized before training (wrong number of rows)");
//     assert(nsamples != 0 && "trainingSamplesConcat must be initialized before training (wrong number of columns)");
// 
//     double stopValCur  = 0;
//     double stopValLast = 0;
//     unsigned int it = 0;
//     std::vector<double> taus(nsamples);
//     std::vector<double> numbernonzeros_mean;
//     std::vector<double> numbernonzeros_stddev;
//     arma::umat usage(natoms, 0);
//     arma::umat usage_count(natoms, 1);
//     for (unsigned int i = 0; i < natoms; ++i)
//         usage_count(i, 0) = i;
//     arma::mat highTrainSamples = highMatView(trainingSamplesConcat);
//     arma::mat lowTrainSamples  = lowMatView(trainingSamplesConcat);
//     arma::mat highValSamples;
//     arma::mat lowValSamples;
//     if (validationSamplesConcat.n_cols > 0) {
//         highValSamples = highMatView(validationSamplesConcat);
//         lowValSamples  = lowMatView(validationSamplesConcat);
//     }
// 
// 
//     SpstfmOptions::GPSROptions const& gpsrOpts = opt.getGPSRTrainingOptions();
// 
//     bool calcObjective       =  opt.dbg_recordTrainingStopFunctions || opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::objective;
//     bool calcObjectiveMaxTau =  opt.dbg_recordTrainingStopFunctions || opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::objective_maxtau;
//     bool calcTestSetError    =  opt.dbg_recordTrainingStopFunctions || opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::test_set_error  || opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::test_set;
//     bool calcTrainSetError   = (opt.dbg_recordTrainingStopFunctions || opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::train_set_error || opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::train_set)
//                              && opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::low;
// 
//     arma::mat coeff(natoms, nsamples);
//     double bestShotVal = std::numeric_limits<double>::infinity();
//     arma::mat bestShotDictConcat;
// //    int bestShotIt = 0; // TODO: currently unused, but planned to be used for logging system
// //    drawDictionary(dictConcat, "dict_untrained.png");
//     bool doStop;
//     do {
//         // get sparse representation coefficients
//         arma::vec nnzs(nsamples);
//         arma::mat dictHigh = highMatView(dictConcat);
//         arma::mat dictLow  = lowMatView(dictConcat);
//         for (unsigned int c = 0; c < nsamples; ++c) {
//             double tau = 0;
//             arma::vec rep;
//             if (opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::concat)
//                 rep = gpsr(trainingSamplesConcat.col(c), dictConcat, gpsrOpts, &tau);
//             else if (opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::high)
//                 rep = gpsr(highTrainSamples.col(c), dictHigh, gpsrOpts, &tau);
//             else if (opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::low)
//                 rep = gpsr(lowTrainSamples.col(c), dictLow, gpsrOpts, &tau);
//             else { // if (opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::average)
//                 double tauHigh, tauLow;
//                 arma::vec repHigh = gpsr(highTrainSamples.col(c), dictHigh, gpsrOpts, &tauHigh);
//                 arma::vec repLow  = gpsr(lowTrainSamples.col(c),  dictLow,  gpsrOpts, &tauLow);
//                 tau = 0.5 * tauHigh + 0.5 * tauLow;
//                 rep = 0.5 * repHigh + 0.5 * repLow;
//             }
// 
//             coeff.col(c) = rep;
// 
//             taus.at(c) = tau;
//             arma::vec nzs = arma::nonzeros(rep);
//             nnzs(c) = nzs.n_elem;
//             if (nzs.n_elem > dim * (opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::concat ? 2 : 1))
//                 Rcpp::Rcout << "Note: In training iteration " << it << " sparse sample patch " << c << " has a large number of non-zero entries: " << nzs.n_elem << " > "
//                           << (opt.getSparseCoeffTrainingResolution() == SpstfmOptions::TrainingResolution::concat ? std::to_string(2*dim) + " = 2 * dim" : std::to_string(dim) + " = dim") << "." << std::endl;
//         }
// 
//         // very first check for stopping criterion
//         if (it == 0 && (opt.getMinTrainIter() < 2 || opt.dbg_recordTrainingStopFunctions || opt.getBestShotErrorSet() != SpstfmOptions::BestShotErrorSet::none)) {
//             if (calcObjective) {
//                 double val;
//                 if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::concat)
//                     val = objective_improved(trainingSamplesConcat, dictConcat, coeff, taus);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::high)
//                     val = objective_improved(highTrainSamples, highMatView(dictConcat), coeff, taus);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::low)
//                     val = objective_improved(lowTrainSamples, lowMatView(dictConcat), coeff, taus);
//                 else // if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::average)
//                     val = 0.5 * (objective_improved(highTrainSamples, highMatView(dictConcat), coeff, taus) + objective_improved(lowTrainSamples, lowMatView(dictConcat), coeff, taus));
// 
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::objective)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions && dbg_objective.empty())
//                     dbg_objective.push_back(val);
//             }
//             if (calcObjectiveMaxTau) {
//                 double maxtau = *std::max_element(taus.begin(), taus.end());
//                 double val;
//                 if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::concat)
//                     val = objective_simple(trainingSamplesConcat, dictConcat, coeff, maxtau);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::high)
//                     val = objective_simple(highTrainSamples, highMatView(dictConcat), coeff, maxtau);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::low)
//                     val = objective_simple(lowTrainSamples, lowMatView(dictConcat), coeff, maxtau);
//                 else // if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::average)
//                     val = 0.5 * (objective_simple(highTrainSamples, highMatView(dictConcat), coeff, maxtau) + objective_simple(lowTrainSamples, lowMatView(dictConcat), coeff, maxtau));
// 
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::objective_maxtau)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions && dbg_objectiveMaxTau.empty())
//                     dbg_objectiveMaxTau.push_back(val);
//             }
//             if (calcTestSetError) {
//                 double val = testSetError(highValSamples, lowValSamples, dictConcat, gpsrOpts, normFactorForHigh);
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::test_set_error)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions && dbg_testSetError.empty())
//                     dbg_testSetError.push_back(val);
//                 if (opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::test_set) {
//                     bestShotVal = val;
//                     bestShotDictConcat = dictConcat;
// //                    bestShotIt = it;
//                 }
//             }
//             if (calcTrainSetError) {
//                 double val = 0;
//                 for (unsigned int c = 0; c < nsamples; ++c)
//                     val += arma::norm(highTrainSamples.col(c) - dictHigh * coeff.col(c), 1);
//                 val *= normFactorForHigh / highTrainSamples.n_elem;
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::train_set_error)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions && dbg_trainSetError.empty())
//                     dbg_trainSetError.push_back(val);
//                 if (opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::train_set) {
//                     bestShotVal = val;
//                     bestShotDictConcat = dictConcat;
// //                    bestShotIt = it;
//                 }
//             }
//         }
// 
//         // save usage pattern
//         arma::uvec use = arma::any(coeff, 1);
// //        Rcpp::Rcout << "Used atoms: " << arma::sum(use) << std::endl;
//         arma::uvec use_count = arma::sum(coeff != 0., 1);
//         usage.insert_cols(usage.n_cols, use);
//         usage_count.insert_cols(usage_count.n_cols, use_count);
// 
// //        /// DEBUG: remove unused atoms immedeately
// //        arma::uvec untrainedAtoms = arma::find(use == 0);
// //        std::string removedAtomsText;
// //        arma::mat untrainedDict(dictConcat.n_rows, untrainedAtoms.n_elem);
// //        int udi = 0;
// //        for (auto k : untrainedAtoms) {
// //            untrainedDict.col(udi++) = dictConcat.col(k);
// //            dictConcat.col(k).zeros();
// //            removedAtomsText += " " + std::to_string(k);
// //        }
// //        if (untrainedDict.n_cols > 0)
// //            drawDictionary(untrainedDict, "untrained_patches" + std::to_string(it) + ".png");     /// DEBUG
// //        if (!removedAtomsText.empty())
// //            Rcpp::Rcout << "Removed untrained atoms:" << removedAtomsText << std::endl;
// 
//         // stats
//         numbernonzeros_mean.push_back(arma::mean(nnzs));
//         numbernonzeros_stddev.push_back(arma::stddev(nnzs));
// 
//         // update dictionary columns
//         if (opt.getColumnUpdateCoefficientResolution() == SpstfmOptions::TrainingResolution::concat)
//             dictConcat = ksvd(trainingSamplesConcat, dictConcat, coeff, opt.useKSVDOnlineMode(), opt.getDictionaryKSVDNormalization());
//         else {
//             auto dicts = doubleKSVD(highTrainSamples, highMatView(dictConcat), lowTrainSamples, lowMatView(dictConcat), coeff, opt.getColumnUpdateCoefficientResolution(), opt.useKSVDOnlineMode(), opt.getDictionaryKSVDNormalization());
//             highMatView(dictConcat) = dicts.first;
//             lowMatView(dictConcat)  = dicts.second;
//         }
// 
// 
//         ++it;
// //        if (it == 0)
// //            drawDictionary(dictConcat, "dict_trained" + std::to_string(it) + ".png");
// 
//         // check for stopping criterion
//         stopValLast = stopValCur;
//         if (it + 1 >= opt.getMinTrainIter() || opt.dbg_recordTrainingStopFunctions || opt.getBestShotErrorSet() != SpstfmOptions::BestShotErrorSet::none) {
//             if (calcObjective) {
//                 double val;
//                 if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::concat)
//                     val = objective_improved(trainingSamplesConcat, dictConcat, coeff, taus);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::high)
//                     val = objective_improved(highTrainSamples, highMatView(dictConcat), coeff, taus);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::low)
//                     val = objective_improved(lowTrainSamples, lowMatView(dictConcat), coeff, taus);
//                 else // if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::average)
//                     val = 0.5 * (objective_improved(highTrainSamples, highMatView(dictConcat), coeff, taus) + objective_improved(lowTrainSamples, lowMatView(dictConcat), coeff, taus));
// 
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::objective)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions)
//                     dbg_objective.push_back(val);
//             }
//             if (calcObjectiveMaxTau) {
//                 double maxtau = *std::max_element(taus.begin(), taus.end());
//                 double val;
//                 if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::concat)
//                     val = objective_simple(trainingSamplesConcat, dictConcat, coeff, maxtau);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::high)
//                     val = objective_simple(highTrainSamples, highMatView(dictConcat), coeff, maxtau);
//                 else if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::low)
//                     val = objective_simple(lowTrainSamples, lowMatView(dictConcat), coeff, maxtau);
//                 else // if (opt.getTrainingStopObjectiveFunctionResolution() == SpstfmOptions::TrainingResolution::average)
//                     val = 0.5 * (objective_simple(highTrainSamples, highMatView(dictConcat), coeff, maxtau) + objective_simple(lowTrainSamples, lowMatView(dictConcat), coeff, maxtau));
// 
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::objective_maxtau)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions)
//                     dbg_objectiveMaxTau.push_back(val);
//             }
//             if (calcTestSetError) {
//                 double val = testSetError(highValSamples, lowValSamples, dictConcat, gpsrOpts, normFactorForHigh);
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::test_set_error)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions)
//                     dbg_testSetError.push_back(val);
//                 if (opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::test_set && bestShotVal > val) {
//                     bestShotVal = val;
//                     bestShotDictConcat = dictConcat;
// //                    bestShotIt = it;
//                 }
//             }
//             if (calcTrainSetError) {
//                 double val = 0;
//                 for (unsigned int c = 0; c < nsamples; ++c)
//                     val += arma::norm(highTrainSamples.col(c) - dictHigh * coeff.col(c), 1);
//                 val *= normFactorForHigh / highTrainSamples.n_elem;
//                 if (opt.getTrainingStopFunction() == SpstfmOptions::TrainingStopFunction::train_set_error)
//                     stopValCur = val;
//                 if (opt.dbg_recordTrainingStopFunctions)
//                     dbg_trainSetError.push_back(val);
//                 if (opt.getBestShotErrorSet() == SpstfmOptions::BestShotErrorSet::train_set && bestShotVal > val) {
//                     bestShotVal = val;
//                     bestShotDictConcat = dictConcat;
// //                    bestShotIt = it;
//                 }
//             }
//         }
// 
//         doStop = false;
//         if (it >= opt.getMinTrainIter()) {
//             if (opt.getTrainingStopCondition() == SpstfmOptions::TrainingStopCondition::val_less)
//                 doStop = stopValCur < opt.getTrainingStopTolerance();
//             else if (opt.getTrainingStopCondition() == SpstfmOptions::TrainingStopCondition::abs_change_less)
//                 doStop = std::abs(stopValLast - stopValCur) < opt.getTrainingStopTolerance();
//             else if (opt.getTrainingStopCondition() == SpstfmOptions::TrainingStopCondition::abs_rel_change_less)
//                 doStop = std::abs((stopValLast - stopValCur) / stopValLast) < opt.getTrainingStopTolerance();
//             else if (opt.getTrainingStopCondition() == SpstfmOptions::TrainingStopCondition::change_less)
//                 doStop = stopValLast - stopValCur < opt.getTrainingStopTolerance();
//             else if (opt.getTrainingStopCondition() == SpstfmOptions::TrainingStopCondition::rel_change_less)
//                 doStop = (stopValLast - stopValCur) / stopValLast < opt.getTrainingStopTolerance();
//         }
//     } while (it < opt.getMinTrainIter() || (it < opt.getMaxTrainIter() && !doStop));
// 
//     // use the best dictionary state, if requested
//     if (opt.getBestShotErrorSet() != SpstfmOptions::BestShotErrorSet::none) {
// //        Rcpp::Rcout << "After " << it << " training iterations using best shot dictionary from it " << bestShotIt << "." << std::endl;
// //                  << " with test set error: " << bestShotVal << std::endl;
//         dictConcat = bestShotDictConcat;
//     }
// 
// 
//     /// DEBUG: try to remove untrained atoms (no effect at all for circle image)
// //    if (trainedAtoms[channel] < natoms) {
// //        arma::uvec untrainedAtoms = arma::find(arma::any(usage, 1) == 0);
// ////        arma::uvec untrainedAtoms = arma::find(arma::all(usage, 1) == 0);     /// DEBUG
// //        std::string removedAtomsText;
// //        arma::mat untrainedDict(dictConcat.n_rows, untrainedAtoms.n_elem);
// //        int udi = 0;
// //        for (auto k : untrainedAtoms) {
// //            if (k >= trainedAtoms[channel]) {
// //                untrainedDict.col(udi++) = dictConcat.col(k);
// //                dictConcat.col(k).zeros();
// //                removedAtomsText += " " + std::to_string(k);
// //            }
// //        }
// //        if (untrainedDict.n_cols > 0)
// //            drawDictionary(untrainedDict, "untrained_patches.png");     /// DEBUG
// 
// //        if (!removedAtomsText.empty())
// //            Rcpp::Rcout << "Removed untrained atoms:" << removedAtomsText << std::endl;
// //        trainedAtoms[channel] = natoms;
// //    }
// 
// //    Rcpp::Rcout << "Usage pattern:" << std::endl << usage << std::endl;
// //    Rcpp::Rcout << "Usage pattern (count):" << std::endl << usage_count << std::endl;
// //    Rcpp::Rcout << "Mean NNZ: [";
// //    for (double nnz : numbernonzeros_mean)
// //        Rcpp::Rcout << nnz << ", ";
// //    Rcpp::Rcout << "];" << std::endl;
// //    Rcpp::Rcout << "Stddev NNZ: [";
// //    for (double nnz : numbernonzeros_stddev)
// //        Rcpp::Rcout << nnz << ", ";
// //    Rcpp::Rcout << "];" << std::endl;
// }
// 
// std::vector<arma::mat> spstfm_impl_detail::DictTrainer::reconstructPatchRow(ConstImage const& high1, ConstImage const& high3, ConstImage const& low1, ConstImage const& low2, ConstImage const& low3, double fillL21, double fillL23, unsigned int pyi, imagefusion::Rectangle sampleArea, unsigned int channel) const {
//     double meanForHigh       = meanForHighDiff_cv[channel];
//     double meanForLow        = meanForLowDiff_cv[channel];
//     double normFactorForHigh = normFactorsHighDiff[channel];
//     double normFactorForLow  = normFactorsLowDiff[channel];
// //        Rcpp::Rcout << "Using norm factor: " << normFactorForHigh << " for high res and " << normFactorForLow << " for low res." << std::endl;
// 
//     unsigned int psize = opt.getPatchSize();
//     unsigned int pover = opt.getPatchOverlap();
//     unsigned int dist = psize - pover;
//     unsigned int npx = (sampleArea.width - pover) / dist;
//     std::vector<arma::mat> patches(npx);
//     arma::vec alpha;
//     arma::mat l21, l23, maskPatch;
//     arma::mat h21(psize, psize);
//     arma::mat h23(psize, psize);
//     arma::mat dictHigh = highMatView(dictsConcat[channel]);
//     arma::mat dictLow  = lowMatView(dictsConcat[channel]);
//     unsigned int maskChannel = sampleMask.channels() > channel ? channel : 0;
// 
//     for (unsigned int pxi = 0; pxi < npx; ++pxi) {
//         assert(weights1.n_rows > pyi &&
//                weights3.n_rows > pyi &&
//                weights1.n_cols > pxi &&
//                weights3.n_cols > pxi);
//         double w1 = weights1(pyi, pxi);
//         double w3 = weights3(pyi, pxi);
// 
//         double sum = w1 + w3;
//         w1 /= sum;
//         w3 /= sum;
// 
//         maskPatch = extractPatch(sampleMask, pxi, pyi, psize, pover, sampleArea, maskChannel);
// 
//         // if all pixels are invalid, skip this patch
//         if (arma::accu(maskPatch == 0) == psize*psize) {
//             patches[pxi] = arma::mat(psize, psize);
//             continue;
//         }
//         arma::uvec invalidIndices = arma::find(maskPatch == 0);
// 
//         // if prediction for non of the pixels of this patch is wanted, skip it
//         if (!writeMask.empty()) {
//             arma::mat predictPatch = extractPatch(writeMask, pxi, pyi, psize, pover, sampleArea, maskChannel);
//             if (arma::accu(predictPatch == 0) == psize*psize) {
//                 patches[pxi] = arma::mat(psize, psize);
//                 continue;
//             }
//         }
// 
//         if (w1 != 0) {
//             l21 = extractPatch(low2, low1, pxi, pyi, psize, pover, sampleArea, channel);
//             l21.elem(invalidIndices).fill(fillL21);
//             l21 -= meanForLow;
//             l21 *= 1 / normFactorForLow;
// 
//             alpha = gpsr(l21, dictLow, opt.getGPSRReconstructionOptions());
//             h21 = dictHigh * alpha;
//             h21 *= normFactorForHigh;
//             h21 += meanForHigh;
//             h21.reshape(psize, psize); // this and all other reshaped vectors will be transposed, because of column-major memory layout in armadillo
//         }
//         else
//             h21.zeros();
// 
//         if (w3 != 0) {
//             l23 = extractPatch(low2, low3, pxi, pyi, psize, pover, sampleArea, channel);
//             l23.elem(invalidIndices).fill(fillL23);
//             l23 -= meanForLow;
//             l23 *= 1 / normFactorForLow;
// 
//             alpha = gpsr(l23, dictLow, opt.getGPSRReconstructionOptions());
//             h23 = dictHigh * alpha;
//             h23 *= normFactorForHigh;
//             h23 += meanForHigh;
//             h23.reshape(psize, psize);
//         }
//         else
//             h23.zeros();
// 
//         arma::mat h1 = extractPatch(high1, pxi, pyi, psize, pover, sampleArea, channel);
//         h1.reshape(psize, psize);
//         arma::mat h3 = extractPatch(high3, pxi, pyi, psize, pover, sampleArea, channel);
//         h3.reshape(psize, psize);
//         arma::mat p = w1 * (h1 + h21) + w3 * (h3 + h23);
//         patches[pxi] = p.t();
//     }
//     return patches;
// }
// 
// void spstfm_impl_detail::DictTrainer::outputAveragedPatchRow(std::vector<arma::mat> const& topPatches, std::vector<arma::mat>& bottomPatches, unsigned int pyi, Rectangle crop, unsigned int npx, unsigned int npy, unsigned int channel) {
//     int psize = opt.getPatchSize();
//     int pover = opt.getPatchOverlap();
//     int dist = psize - pover;
// 
//     int outy = dist * pyi - crop.y;
//     int outheight = output.height() - std::max(outy, 0);
//     if (outy <= -psize + pover || outheight <= 0)
//         return;
// 
//     bool isfirstrow = pyi == 0; // in this case topPatches is empty, bottomPatches is the first row
//     bool islastrow = pyi == npy - 1;
//     arma::mat dummy(1, 1);
// 
//     // put left border into the image
//     if (psize - crop.x - pover > 0) {
//         // only output left border if there is a contribution
//         if (isfirstrow) {
//             // in the top row, the most left patch does not need averaging
//             arma::subview<double> patchTL = bottomPatches[0](crop.y, crop.x,
//                                                              arma::size(std::min(psize - crop.y - pover, output.height()),
//                                                                         std::min(psize - crop.x - pover, output.width())));
// 
//             imagefusion::CallBaseTypeFunctor::run(CopyFromToFunctor<arma::subview<double>>{patchTL, output, channel}, output.basetype());
//         }
//         else {
//             // modify the top left of the bottom left border patch and output it
//             arma::subview<double> leftBorder = bottomPatches[0].cols(crop.x, psize - pover - 1); // note: right overlapping region will not be modified
//             if (pover > 0) {
//                 arma::subview<double> leftBorderOverlap = leftBorder.rows(0, pover - 1);
//                 leftBorderOverlap += topPatches[0](psize - pover, crop.x, arma::size(pover, psize - crop.x - pover));
//                 leftBorderOverlap *= 0.5;
//             }
// 
//             int firstrow = 0;
//             int lastrow = leftBorder.n_rows - 1;
//             if (outy < 0)
//                 firstrow = -outy;
//             if (outheight <= lastrow - firstrow)
//                 lastrow = firstrow + outheight - 1;
// 
//             // check if output image is less wide than 1 patch
//             int lastcol = leftBorder.n_cols - 1;
//             if (leftBorder.n_cols > static_cast<unsigned int>(output.width()))
//                 lastcol = output.width() - 1;
// 
//             Image outleft{output.sharedCopy(imagefusion::Rectangle(0, std::max(outy, 0), output.width(), outheight))};
//             imagefusion::CallBaseTypeFunctor::run(CopyFromToFunctor<arma::subview<double>>{leftBorder.submat(firstrow, 0, lastrow, lastcol), outleft, channel}, output.basetype());
//         }
//     }
// 
//     // output the (bottom) right patch
//     for (unsigned int pxi = 1; pxi < npx; ++pxi) {
//         bool islastcol = pxi == npx - 1;
// 
//         // get output patch (maybe larger than the resulting patch)
//         int outx = dist * static_cast<int>(pxi) - crop.x;
//         int outwidth  = output.width()  - std::max(outx, 0);
//         if (outx <= -psize + pover || outwidth <= 0)
//             continue;
//         Image outpatch{output.sharedCopy(imagefusion::Rectangle(std::max(outx, 0), std::max(outy, 0), outwidth, outheight))};
// 
//         // get relevant regions of patches
//         // since the BR patch will be modified in the overlapping region, we modify only the part without the right and the bottom overlap region
//         arma::subview<double> patchBR = bottomPatches[pxi].submat(0, 0,
//                                                                   islastrow ? psize - 1 : psize - pover - 1,
//                                                                   islastcol ? psize - 1 : psize - pover - 1);
//         if (pover > 0) {
//             arma::subview<double> patchBL = bottomPatches[pxi-1].submat(0,
//                                                                         psize - pover,
//                                                                         islastrow ? psize - 1 : psize - pover - 1,
//                                                                         psize - 1);
//             arma::subview<double> patchTL = isfirstrow ? dummy.submat(0, 0, 0, 0) : topPatches[pxi-1](psize - pover, psize - pover, arma::size(pover, pover));
//             arma::subview<double> patchTR = isfirstrow ? dummy.submat(0, 0, 0, 0) : topPatches[pxi].submat(psize - pover, 0,
//                                                                                                            psize - 1,
//                                                                                                            islastcol ? psize - 1 : psize - pover - 1);
// 
//             // top
//             if (!isfirstrow)
//             {
//                 arma::subview<double> BRtop = patchBR.rows(0, pover - 1);
//                 BRtop += patchTR;
//                 BRtop.cols(pover, BRtop.n_cols - 1) *= 0.5;
//             }
// 
//             // left
//             {
//                 arma::subview<double> BRleft = patchBR.cols(0, pover - 1);
//                 BRleft += patchBL;
//                 if (!isfirstrow)
//                     BRleft.rows(pover, BRleft.n_rows - 1) *= 0.5;
//                 else
//                     BRleft *= 0.5;
//             }
// 
//             // top-left
//             if (!isfirstrow)
//             {
//                 arma::subview<double> BRtopleft = patchBR.submat(0, 0, pover - 1, pover - 1);
//                 BRtopleft += patchTL;
//                 BRtopleft *= 0.25;
//             }
//         }
// 
//         // put out patch
//         int firstrow = 0;
//         int lastrow = patchBR.n_rows - 1;
//         int firstcol = 0;
//         int lastcol = patchBR.n_cols - 1;
//         if (outx < 0)
//             firstcol = -outx;
//         if (outy < 0)
//             firstrow = -outy;
//         if (outwidth <= lastcol - firstcol)
//             lastcol = firstcol + outwidth - 1;
//         if (outheight <= lastrow - firstrow)
//             lastrow = firstrow + outheight - 1;
//         CallBaseTypeFunctor::run(CopyFromToFunctor<arma::subview<double>>{patchBR.submat(firstrow, firstcol, lastrow, lastcol), outpatch, channel}, output.basetype());
//     }
// }
// 
// void spstfm_impl_detail::DictTrainer::reconstructImage(
//         ConstImage const& high1,
//         ConstImage const& high3,
//         ConstImage const& low1,
//         ConstImage const& low2,
//         ConstImage const& low3,
//         double fillL21, double fillL23,
//         Rectangle predArea, Rectangle sampleArea,
//         unsigned int channel)
// {
//     Rectangle crop = predArea;
//     crop.x = predArea.x - sampleArea.x;
//     crop.y = predArea.y - sampleArea.y;
// 
//     int psize = opt.getPatchSize();
//     int pover = opt.getPatchOverlap();
//     int dist = psize - pover;
//     int npx = (sampleArea.width  - pover) / dist;
//     int npy = (sampleArea.height - pover) / dist;
//     std::vector<arma::mat> topPatches;
//     std::vector<arma::mat> bottomPatches(npx);
//     for (int pyi = 0; pyi < npy; ++pyi) {
//         // reconstruct next line of patches
// //        Rcpp::Rcout << "reconstructing line " << pyi << std::endl;
//         std::swap(topPatches, bottomPatches);
//         bottomPatches = reconstructPatchRow(high1, high3, low1, low2, low3, fillL21, fillL23, pyi, sampleArea, channel);
// 
//         // average to output
// //        Rcpp::Rcout << "averaging..." << std::endl;
//         outputAveragedPatchRow(topPatches, bottomPatches, pyi, crop, npx, npy, channel);
//     }
// 
// }
// 
// 
// 
// } /* namespace imagefusion */
