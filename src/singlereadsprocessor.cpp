//
//  singlereadsprocessor.cpp
//  barcode_project
//
//  Created by luzhao on 1/1/16.
//  Copyright © 2016 luzhao. All rights reserved.
//

#include "singlereadsprocessor.hpp"

#include "pattern.h"
#include "sequence.h"
#include "util.h"
#include "UmiExtractor.hpp"

#include <string>
#include <sstream>
#include <iostream>
namespace barcodeSpace {
    SingleReadsProcessor::SingleReadsProcessor(const std::string& reads_file_name,
                                               std::shared_ptr<BarcodeExtractor> barcodeExtractor,
                                               std::shared_ptr<UmiExtractor> umiExtractor,
                                               file_format format,
                                               const std::string& output,
                                               double qual_thres) :
                            _barcodeExtractor(barcodeExtractor), _umiExtractor(umiExtractor),
                            _formats(format), _outprefix(output),
                            _barcode_dumper(output + "_barcode.txt", false),
                            _total_reads(0), _total_barcodes(0),
                            _total_valid_barcodes(0), _quality_threshold(qual_thres){
                                _pattern_handler.reset(CreatePatternParser(reads_file_name, format));
                                if (_umiExtractor.get() != nullptr) {
                                    if (_umiExtractor->getUmiConfigs().size() != 1) {
                                        throw new runtime_error("Bartender now only support only one umi!");
                                    }
                                }
    }
    
    void SingleReadsProcessor::extract() {
        bool done = false;
        bool success = false;
        Sequence read;
        int firstPartStart = 0;
        int firstPartEnd = _umiExtractor->getUmiConfigs()[0].getUmiPosition();
        
        int secondPartStart =_umiExtractor->getUmiConfigs()[0].getUmiPosition() + _umiExtractor->getUmiConfigs()[0].getUmiLength();
        
        
        while(!done) {
            read.clear();
            size_t line = _pattern_handler->CurrentLine();
            _pattern_handler->parse(read, success, done);
            if (success) {
                // extract umi first.
                string umi;
                // if needs to extrac umi, we need to split the reads into two separate part
                // and try to extract barcode from each of them.
                // And we assume that each read could only have one barcode.
                // That means, we will skip this read if we can find two or more barcodes in it.
                if (_umiExtractor.get() != nullptr) {
                    umi = _umiExtractor->extractUmi(read);
                    Sequence previous = read.subRead(firstPartStart, firstPartEnd);
                    Sequence after = read.subRead(secondPartStart, read.length() - secondPartStart);
                    ExtractionResultType returnTypePrevious = _barcodeExtractor->ExtractBarcode(previous);
                    ExtractionResultType returnTypeAfter = _barcodeExtractor->ExtractBarcode(after);
                    
                    // This read has at lease two barcode.
                    // log this read and skil it
                    if (returnTypePrevious != FAIL && returnTypeAfter != FAIL) {
                        std::cerr << "read: " << read.fowardSeq() << " at line " << line << " has at lease two barcodes" << std::endl;
                        std::cerr << "This read will be skipped!" << std::endl;
                    } else {
                        if (returnTypePrevious == FAIL) {
                            if (returnTypeAfter != FAIL) {
                                if (returnType == REVERSE_COMPLEMENT) {
                                    reverseComplementInplace(umi);
                                }
                                this->extractAndLogCount(after, line, umi);
                            }
                        } else {
                            if (returnType == REVERSE_COMPLEMENT) {
                                reverseComplementInplace(umi);
                            }
                            this->extractAndLogCount(previous, line, umi);
                        }
                    }
                } else {
                    // If get a read successfully, then extract the barcode from the read.
                    ExtractionResultType returnType = _barcodeExtractor->ExtractBarcode(read);
                    // If extracted a barcode from the read successfully,
                    if (returnType != FAIL) {
                        if (returnType == REVERSE_COMPLEMENT) {
                            reverseComplementInplace(umi);
                        }
                        this->extractAndLogCount(read, line, umi);
                    }
                }
		       // keep tract all parsed reads.
                ++_total_reads;
            }
        }

    }
    void SingleReadsProcessor::extractAndLogCount(const Sequence& read, const size_t lineNumber, const string& umi) {
        std::stringstream ss;
        // The average quality is above the threshold.
        if (qualityCheck(read.quality(), _quality_threshold)) {
            if (umi.empty()) {
                ss << read.fowardSeq() << ',' << lineNumber << std::endl;
            } else {
                ss << read.fowardSeq() << ',' << umi << std::endl;
            }
            _barcode_dumper.writeString(ss.str());
            // Keep tract all valid barcodes that pass the quality check.
            ++_total_valid_barcodes;
        }
        // keep track all read that has valid barcode in it
        ++_total_barcodes;
    }
}   // namespace barcodeSpace
