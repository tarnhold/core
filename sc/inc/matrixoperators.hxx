/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SC_INC_MATRIXOPERATORS_HXX
#define INCLUDED_SC_INC_MATRIXOPERATORS_HXX


#include <functional>

namespace sc::op {


template<typename T>
struct Op_
{
    const double mInitVal;
    const T maOp;
    Op_(double InitVal, T aOp):
        mInitVal(InitVal), maOp(aOp)
    {
    }
    void operator()(double& rAccum, double fVal) const
    {
        maOp(rAccum, fVal);
    }
};

using Op = Op_<std::function<void(double&, double)>>;

struct Sum
{
    static const double InitVal;
    void operator()(double& rAccum, double fVal) const;
};

struct SumSquare
{
    static const double InitVal;
    void operator()(double& rAccum, double fVal) const;
};

struct Product
{
    static const double InitVal;
    void operator()(double& rAccum, double fVal) const;
};

}



#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
