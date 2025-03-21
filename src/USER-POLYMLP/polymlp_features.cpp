/**************************************************************************** 

        Copyright (C) 2020 Atsuto Seko
                seko@cms.mtl.kyoto-u.ac.jp

        This program is free software; you can redistribute it and/or
        modify it under the terms of the GNU General Public License
        as published by the Free Software Foundation; either version 2
        of the License, or (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to
        the Free Software Foundation, Inc., 51 Franklin Street,
        Fifth Floor, Boston, MA 02110-1301, USA, or see
        http://www.gnu.org/copyleft/gpl.txt

*****************************************************************************/

/*****************************************************************************

        SingleTerm: [coeff,[(n1,l1,m1,tc1), (n2,l2,m2,tc2), ...]]
            (n1,l1,m1,tc1) is represented with nlmtc_key.

        SingleFeature: [SingleTerms1, SingleTerms2, ...]

        MultipleFeatures: [SingleFeature1, SingleFeature2, ...]

*****************************************************************************/

#include "polymlp_features.h"

Features::Features(){}

Features::Features(const feature_params& fp, const ModelParams& modelp){

    n_fn = fp.params.size();
    n_tc = modelp.get_n_type_comb();

    MultipleFeatures mfeatures1;
    if (fp.des_type == "pair"){
        set_mapping_ntc();
        mfeatures = set_linear_features_pair();
    }
    else if (fp.des_type == "gtinv"){
        set_mapping_lm(fp.maxl);
        set_mapping_nlmtc();
        mfeatures = set_linear_features(fp, modelp);
    }

    // this part can be revised in a recursive form
    for (int i = 0; i < mfeatures.size(); ++i){
        const vector1i c = {i};
        feature_combinations.emplace_back(c);
    }
    const auto& comb2 = modelp.get_comb2();
    const auto& comb3 = modelp.get_comb3();
    for (const auto& c: comb2) feature_combinations.emplace_back(c);
    for (const auto& c: comb3) feature_combinations.emplace_back(c);

}

Features::~Features(){}

MultipleFeatures Features::set_linear_features_pair(){

    // The order of tc and n must be fixed.
    std::vector<SingleFeature> feature_array;
    for (int tc = 0; tc < n_tc; ++tc){
        for (int n = 0; n < n_fn; ++n){
            SingleFeature feature;
            SingleTerm single;
            single.coeff = 1.0;
            const int key = mapping_ntc_to_key(tc, n);
            single.nlmtc_keys.emplace_back(key);
            feature.emplace_back(single);
            feature_array.emplace_back(feature);
        }
    }
    return feature_array;
}

MultipleFeatures Features::set_linear_features(const feature_params& fp, 
                                               const ModelParams& modelp){

    const vector3i& lm_array = fp.lm_array;
    const vector2d& lm_coeffs = fp.lm_coeffs;

    // The order of n and linear must be fixed.
    std::vector<SingleFeature> feature_array;
    for (int n = 0; n < n_fn; ++n){
        for (const auto& linear: modelp.get_linear_term_gtinv()){
            const int lm_idx = linear.lmindex;
            const auto& tc_array = linear.tcomb_index;
            const auto& lm_list = lm_array[lm_idx];
            const auto& coeff_list = lm_coeffs[lm_idx];

            SingleFeature feature;
            for (int i = 0; i < lm_list.size(); ++i){
                SingleTerm single;
                single.coeff = coeff_list[i];
                for (int j = 0; j < lm_list[i].size(); ++j){
                    const auto lm = lm_list[i][j];
                    const auto tc = tc_array[j];
                    const int key = mapping_nlmtc_to_key(tc, lm, n);
                    single.nlmtc_keys.emplace_back(key);
                }
                std::sort(single.nlmtc_keys.begin(), single.nlmtc_keys.end());
                feature.emplace_back(single);
            }
            feature_array.emplace_back(feature);
        }
    }
    return feature_array;
}

int Features::mapping_nlmtc_to_key(const int tc, const int lm, const int n){
    return n * (n_lm_all * n_tc) + lm * n_tc + tc;
}

int Features::mapping_ntc_to_key(const int tc, const int n){
    return n * n_tc + tc;
}

// for des_type == pair
void Features::set_mapping_ntc(){

    int ntc_key(0);
    for (int n = 0; n < n_fn; ++n){
        for (int tc = 0; tc < n_tc; ++tc){
            ntcAttribute ntc = {n, tc, ntc_key};
            ntc_map.emplace_back(ntc);
            ++ntc_key;
        }
    }
}

// for des_type == gtinv
void Features::set_mapping_nlmtc(){

    int nlmtc_key(0), nlmtc_noconj_key(0), conj_key, conj_key_add;
    for (int n = 0; n < n_fn; ++n){
        for (int lm = 0; lm < n_lm_all; ++lm){
            const auto& lm_attr = lm_map[lm];
            conj_key_add = 2 * lm_attr.m * n_tc;
            for (int tc = 0; tc < n_tc; ++tc){
                conj_key = nlmtc_key - conj_key_add;
                nlmtcAttribute nlmtcs = {n, lm_attr, tc, nlmtc_key, conj_key, 
                                         nlmtc_noconj_key};
                nlmtc_map.emplace_back(nlmtcs);
                ++nlmtc_key;
                if (lm_attr.conj == false) {
                    nlmtc_map_no_conjugate.emplace_back(nlmtcs);
                    ++nlmtc_noconj_key;
                }
            }
        }
    }
    n_nlmtc_all = n_tc * n_lm_all * n_fn;
}

void Features::set_mapping_lm(const int maxl){

    int ylm_key;
    double cc, sign_j;
    bool conj;
    for (int l = 0; l < maxl + 1; ++l){
        if (l % 2 == 0){
            sign_j = 1;
        }
        else {
            sign_j = -1;
        }
        for (int m = -l; m < l + 1; ++m){
            if (m % 2 == 0) {
                cc = 1;
            }
            else {
                cc = -1;
            }
            if (m < 1) {
                ylm_key = (l+3)*l/2 + m;
                conj = false;
            }
            else {
                ylm_key = (l+3)*l/2 - m;
                conj = true;
            }
            lmAttribute lm_attr = {l, m, ylm_key, conj, cc, sign_j};
            lm_map.emplace_back(lm_attr);
        }
    }

    n_lm_all = lm_map.size();
    n_lm = (n_lm_all + maxl + 1) / 2;
}

const int Features::get_n_features() const {
    return mfeatures.size(); 
}
const int Features::get_n_nlmtc_all() const { 
    return n_nlmtc_all; 
}
const MultipleFeatures& Features::get_features() const { 
    return mfeatures; 
}
const std::vector<lmAttribute>& Features::get_lm_map() const { 
    return lm_map; 
}
const std::vector<nlmtcAttribute>& 
Features::get_nlmtc_map_no_conjugate() const { 
    return nlmtc_map_no_conjugate; 
}
const std::vector<nlmtcAttribute>& Features::get_nlmtc_map() const { 
    return nlmtc_map; 
}
const std::vector<ntcAttribute>& Features::get_ntc_map() const { 
    return ntc_map; 
}
const vector2i& Features::get_feature_combinations() const { 
    return feature_combinations; 
}

// not used
SingleFeature Features::product_features(const SingleFeature& feature1, 
                                         const SingleFeature& feature2){
    SingleFeature feature;
    for (const auto& term1: feature1){
        for (const auto& term2: feature2){
            SingleTerm single;
            single.coeff = term1.coeff * term2.coeff;
            for (const auto& key: term1.nlmtc_keys){
                single.nlmtc_keys.emplace_back(key);
            }
            for (const auto& key: term2.nlmtc_keys){
                single.nlmtc_keys.emplace_back(key);
            }
            std::sort(single.nlmtc_keys.begin(), single.nlmtc_keys.end());
            feature.emplace_back(single);
        }
    }
    return feature;
}

