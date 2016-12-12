/*
 * Copyright 2013-2015 Raphael Bost
 *
 * This file is part of ciphermed.

 *  ciphermed is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  ciphermed is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with ciphermed.  If not, see <http://www.gnu.org/licenses/>. 2
 *
 */

#include <mpc/lsic.hh>
#include <mpc/private_comparison.hh>
#include <mpc/enc_comparison.hh>
#include <mpc/rev_enc_comparison.hh>
#include <mpc/linear_enc_argmax.hh>

#include <classifiers/nb_classifier.hh>

#include <protobuf/protobuf_conversion.hh>
#include <net/message_io.hh>
#include <util/util.hh>

static const COMPARISON_PROTOCOL comparison_prot__ = GC_PROTOCOL;

Naive_Bayes_Classifier_Server::Naive_Bayes_Classifier_Server(gmp_randstate_t state, unsigned int keysize, unsigned int lambda, const vector<vector<double>> &conditionals_vec, const vector<double> &prior_vec)
: Server(state, Naive_Bayes_Classifier_Server::key_deps_descriptor(), keysize, lambda)
{
    // initialize the model arrays
    enc_prior_vec_ = vector<mpz_class>(prior_vec.size());
    enc_conditionals_vec_ =     vector<vector<mpz_class>>(conditionals_vec.size());

    for (size_t i = 0; i < enc_conditionals_vec_.size(); i++) {
        enc_conditionals_vec_[i] = vector<mpz_class>(conditionals_vec[i].size());
        //enc_conditionals_vec_[i] = vector<vector<mpz_class>>(conditionals_vec[i].size());
        
        //for (size_t j = 0; j < enc_conditionals_vec_[i].size(); j++) {
        //    enc_conditionals_vec_[i][j] = vector<mpz_class>(conditionals_vec[i][j].size());

        //}
    }
    
    prepare_model(conditionals_vec,prior_vec);
    encrypt_model();
}

void Naive_Bayes_Classifier_Server::prepare_model(const vector<vector<double>> &conditionals_vec, const vector<double> &prior_vec)
{
    // first find the max exponent
    int e_max = INT_MIN;
    int e;
    
    for (size_t i = 0; i < prior_vec.size(); i++) {
        frexp(prior_vec[i], &e);
        e_max = max<int>(e,e_max);
    }
    
    for (size_t i = 0; i < conditionals_vec.size(); i++) {
        for (size_t j = 0; j < conditionals_vec[i].size(); j++) {
            frexp(conditionals_vec[i][j], &e);
            e_max = max<int>(e,e_max);
            //for (size_t k = 0; k < conditionals_vec[i][j].size(); k++) {
            //    frexp(conditionals_vec[i][j][k], &e);
            //    e_max = max<int>(e,e_max);
            //}
        }
    }
    
    // now, convert everything into multi-precision integers
    mpf_class temp;
    
    for (size_t i = 0; i < prior_vec.size(); i++) {
        temp = prior_vec[i];
        mpf_mul_2exp(temp.get_mpf_t(),temp.get_mpf_t(),e_max+52);
        enc_prior_vec_[i] = trunc(temp);
    }
    
    for (size_t i = 0; i < conditionals_vec.size(); i++) {
        for (size_t j = 0; j < conditionals_vec[i].size(); j++) {
            temp = conditionals_vec[i][j];
            mpf_mul_2exp(temp.get_mpf_t(),temp.get_mpf_t(),e_max+52);
            enc_conditionals_vec_[i][j] = trunc(temp);

            //for (size_t k = 0; k < conditionals_vec[i][j].size(); k++) {

            //    temp = conditionals_vec[i][j][k];
            //    mpf_mul_2exp(temp.get_mpf_t(),temp.get_mpf_t(),e_max+52);
            //    
            //    enc_conditionals_vec_[i][j][k] = trunc(temp);

            //}
        }
    }
}


void Naive_Bayes_Classifier_Server::encrypt_model()
{
    for (size_t i = 0; i < enc_prior_vec_.size(); i++) {
        enc_prior_vec_[i] = paillier_->encrypt(enc_prior_vec_[i]);
    }
    
    for (size_t i = 0; i < enc_conditionals_vec_.size(); i++) {
        for (size_t j = 0; j < enc_conditionals_vec_[i].size(); j++) {
            enc_conditionals_vec_[i][j] = paillier_->encrypt(enc_conditionals_vec_[i][j]);
            //for (size_t k = 0; k < enc_conditionals_vec_[i][j].size(); k++) {
            //    enc_conditionals_vec_[i][j][k] = paillier_->encrypt(enc_conditionals_vec_[i][j][k]);
            //}
        }
    }
}

Server_session* Naive_Bayes_Classifier_Server::create_new_server_session(tcp::socket &socket)
{
    return new Naive_Bayes_Classifier_Server_session(this, rand_state_, n_clients_++, socket);
}

void Naive_Bayes_Classifier_Server_session::run_session()
{
    try {
        exchange_keys();
        
        ScopedTimer *t;
        RESET_BYTE_COUNT
        RESET_BENCHMARK_TIMER

        // send the encrypted probabilities
        Protobuf::BigIntArray prior_prob_message = convert_to_message(nb_server_->enc_prior_prob());
        sendMessageToSocket(socket_, prior_prob_message);

        Protobuf::BigIntMatrix cond_prob_message = convert_to_message(nb_server_->enc_cond_prob());
        sendMessageToSocket(socket_, cond_prob_message);
        
        // help for the encrypted argmax
        unsigned int cat_count = nb_server_->categories_count();
        cout << "Categories count " << cat_count << endl;
        
        unsigned int features_count = nb_server_->features_count();
        
        Tree_EncArgmax_Helper helper(54+cat_count,cat_count,server_->paillier());
        run_tree_enc_argmax(helper,comparison_prot__);
        
//        Linear_EncArgmax_Helper helper(54+features_count,cat_count,server_->paillier());
//        run_linear_enc_argmax(helper,comparison_prot__);
        
#ifdef BENCHMARK
        cout << "Benchmark: " << GET_BENCHMARK_TIME << " ms" << endl;
        cout << IOBenchmark::byte_count() << " exchanged bytes" << endl;
        cout << IOBenchmark::interaction_count() << " interactions" << endl;
#endif

    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    
    delete this;
}


Naive_Bayes_Classifier_Client::Naive_Bayes_Classifier_Client(boost::asio::io_service& io_service, gmp_randstate_t state, unsigned int keysize, unsigned int lambda, const vector<unsigned int> &features_value)
: Client(io_service,state,Naive_Bayes_Classifier_Server::key_deps_descriptor(),keysize,lambda), features_value_(features_value)
{
    
}

bool Naive_Bayes_Classifier_Client::run()
{
    // get public keys
    RESET_BYTE_COUNT
    exchange_keys();
#ifdef BENCHMARK
    const double to_kB = 1 << 10;
    cout << "Key exchange: " <<  (IOBenchmark::byte_count()/to_kB) << " kB" << endl;
    cout << IOBenchmark::interaction_count() << " interactions" << endl;
#endif
    
    ScopedTimer *t;
    RESET_BYTE_COUNT
    RESET_BENCHMARK_TIMER
    
    // get the prior and the conditionnal probabilities
    t = new ScopedTimer("Model transmission");
    enc_prior_vec_ = convert_from_message(readMessageFromSocket<Protobuf::BigIntArray>(socket_));
    enc_conditionals_vec_ = convert_from_message(readMessageFromSocket<Protobuf::BigIntMatrix>(socket_));
    delete t;
    
    if (features_value_.size() == 0) {
        generate_random_feature_values();
    }
    
    t = new ScopedTimer("Prob computation");
    
    vector<mpz_class> cat_prob = cat_probabilities();

    delete t;
    
    unsigned int features_count = enc_conditionals_vec_[0].size();

    unsigned int cat_count = cat_prob.size();
    t = new ScopedTimer("Argmax");
    
    Tree_EncArgmax_Owner owner(cat_prob,54+cat_count,*server_paillier_,rand_state_, lambda_);
    size_t output = run_tree_enc_argmax(owner,comparison_prot__);
    cout << "Output: " << output << endl;

//    Linear_EncArgmax_Owner owner(cat_prob,54+features_count,*server_paillier_,rand_state_, lambda_);
//    run_linear_enc_argmax(owner,comparison_prot__);

    delete t;
    
#ifdef BENCHMARK
    cout << "Benchmark: " << GET_BENCHMARK_TIME << " ms" << endl;
    cout << (IOBenchmark::byte_count()/to_kB) << " exchanged kB" << endl;
    cout << IOBenchmark::interaction_count() << " interactions" << endl;
#endif
    return true;
}

void Naive_Bayes_Classifier_Client::generate_random_feature_values()
{
    assert(features_value_.size() == 0);
    
    unsigned int features_count = enc_conditionals_vec_[0].size();
    features_value_ = vector<unsigned int>(features_count);
    
    for (size_t i = 0; i < features_count; i++) {
        //assert(enc_conditionals_vec_[0][i].size() == 2);
        if (rand() % 100 == 0) {
            features_value_[i] = 1;
        } else {
            features_value_[i] = 0;
        }
        //features_value_[i] = rand() % enc_conditionals_vec_[0][i].size();
    }
}

vector<mpz_class> Naive_Bayes_Classifier_Client::cat_probabilities() const
{
    // for each category, compute the probabily to be in this category given the features values
    
    vector<mpz_class> cat_prob(enc_prior_vec_);
    
    for (size_t i = 0; i < cat_prob.size(); i++) {
        // loop over the categories
        
        for (size_t j = 0; j < enc_conditionals_vec_[0].size(); j++) {
            // loop over binary features
            
            unsigned int val = features_value_[j];
            if (val == 1) {
                cat_prob[i] = server_paillier_->add(cat_prob[i],enc_conditionals_vec_[i][j]);
            }
            // cat_prob[i] = cat_prob[i] + enc_prior_vec_[i][j][val]
            // cat_prob[i] = server_paillier_->add(cat_prob[i],enc_conditionals_vec_[i][j][val]);
        }
    }
    
    return cat_prob;
}
