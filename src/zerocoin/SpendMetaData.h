/**
* @file       SpendMetaData.h
*
* @brief      SpendMetaData class for the Zerocoin library.
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       June 2013
*
* @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/

#ifndef SPENDMETADATA_H_
#define SPENDMETADATA_H_

#include "../uint256.h"
#include "../serialize.h"

using namespace std;
namespace libzerocoin {

/** Any meta data needed for actual bitcoin integration.
 * Can extended provided the getHash() function is updated
 */
class SpendMetaData {
public:
	/**
	 * Creates meta data associated with a coin spend
	 * @param accumulatorId hash of block containing accumulator
	 * @param txHash hash of tra