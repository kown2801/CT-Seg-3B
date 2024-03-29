#include "Patrick/Integrators.h"
#include "Patrick/Hyb.h"
#include "Patrick/Plaquette/Plaquette.h"
#include "IO.h"
/*****************************************************************************/
/* Alters the new Hyb file that comes out of the self-consistency relations. */
/* This is used to add constraints to the model (for example pphi and mphi are forced to be real)*/
void hyb_constraints(std::string const component,json& jComponent){
    if(component == "pphi" || component == "mphi"){
        for(size_t i = 0;i < jComponent["imag"].size();i++){
            jComponent["imag"][i] = 0;
        }
    }
}
/*****************************************************************************/
/**************************************************************************/
/* Initializes the selfEnergy when starting a new simulation from scratch */
/* This function may use all the parameter loaded in jParams              */
void initial_self_energy(json& jParams,int n,std::map<std::string,std::complex<double> >& component_map){
    double const delta = jParams["delta"];
	double const beta = jParams["beta"];
    double omega = (2*n + 1)*M_PI/beta;
    component_map["pphi"] = delta/(1. + omega*omega);
    component_map["mphi"] = -delta/(1. + omega*omega);
}
/**************************************************************************/
/***************************************************************************/
/* Initializes the hyb moments when starting a new simulation from scratch */
/* This function may use all the parameter loaded in jParams               */
void initial_Hyb_moments(json& jHyb,json& jParams){
    double tpd = jParams["tpd"];
    jHyb["00"]["First Moment"] = 4*tpd*tpd; 
    jHyb["01"]["First Moment"] = -tpd*tpd; 
    /* Non-explicited First and Second Moment are assumed equal to 0 */
}
/***************************************************************************/
/****************************************************/
/* Post processing of the simple double observables */
void readScalSites(std::string obs, json& jMeas, int iteration,std::string outputFolder) {
    std::ofstream file((outputFolder + obs + "Sites.dat").c_str(), std::ios_base::out | std::ios_base::app);                
    file << iteration; 
    
    for(int i = 0; i < 4; ++i) {
        std::string s = std::to_string(i); 
        file << " " << jMeas[obs + "_" + s][0];
    }
    
    file << std::endl;
    file.close();
    
    file.open((outputFolder + obs + ".dat").c_str(), std::ios_base::out | std::ios_base::app);
    file << iteration << " " << jMeas[obs][0] << std::endl; 
    file.close();
}
/****************************************************/
/************************************************************************************/
/* Saves the data of matrix into the writeDat object that is used to save json data */
void addMatsubaraDataToJson(json& jObject, std::map<std::string,std::vector<std::pair<std::size_t,std::size_t> > >& inverse_component_map,RCuMatrix& matrix){
    //We need to mean on all the indices included in the inverse component map
    //For each component type (00,01,11...) , we add the contributions of all the matrix coefficients that orrespond to those components
    std::size_t nSite_ = matrix.size()/2;
    for (auto &p : inverse_component_map)
    {
        
        std::complex<double> component_mean(0.,0.);
        std::size_t multiplicity = 0;
        if(p.first != "empty"){
            for(auto& pair : p.second){
                //Be careful of the Nambu convention
                if(pair.first >= nSite_ && pair.second >= nSite_){
                    component_mean += -std::conj(matrix(pair.first,pair.second));
                }else{
                    component_mean += matrix(pair.first,pair.second);
                }
                multiplicity+=1;
            }
            component_mean/=multiplicity;
            jObject[p.first]["real"].push_back(component_mean.real());
            jObject[p.first]["imag"].push_back(component_mean.imag());
        }
    }
}
/************************************************************************************/

/***************************************************/
/* This scripts does the self-consistency relations*/
/* It also post-processes the observables, adding the sign and saving them files for future use */
int main(int argc, char** argv)
{
    try {
        if(argc != 6) 
            throw std::runtime_error("Usage : GFULL inputFolder outputFolder dataFolder inputFilename iteration");
        /******************************/
        /* Initialisation of variable */
        std::string inputFolder = argv[1];
        std::string outputFolder = argv[2];
        std::string dataFolder = argv[3];
        std::string name = argv[4];
        int const iteration = std::atoi(argv[5]);
        std::string filename = "";
        if(iteration){
            filename = inputFolder + name + std::to_string(iteration) + ".meas.json";
        }else{
            filename = inputFolder + name + "0.json";
        }
        /******************************/
        /*   Reading the parameters   */
        /******************************/
        json jInputFile;
        json jParams;
		IO::readJsonFile(filename,jInputFile);
        if(iteration){
			jParams = jInputFile["Parameters"];
		}else{
            jParams = jInputFile;
        }
		double const mu = jParams["mu"];
        double const beta = jParams["beta"];
        double const tpd = jParams["tpd"];
        double const tpp = jParams["tpp"];
        double const ep = jParams["ep"];
        double tppp = tpp;
		if(exists(jParams,"tppp")){
			tppp = jParams["tppp"];
            std::cout << "We have tppp different than tpp" << std::endl;
		}
        /******************************/
        std::complex<double> w = .0;
        
        std::vector<RCuMatrix> selfEnergy;
        std::vector<RCuMatrix> hyb;
            
        /***************************/
        /* We read the Link file   */
        json jLink;       
        IO::readLinkFromParams(jLink, outputFolder, jParams);
        std::size_t nSite_ = jLink.size()/2;
		/***************************/
        /*****************************************************************/
        /* Now we create the map object that will contain the components */
        std::map<std::string,std::complex<double> > component_map;
        std::map<std::string,std::vector<std::pair<std::size_t,std::size_t> > > inverse_component_map;
        for(std::size_t i=0;i<jLink.size();i++){
            for(std::size_t j=0;j<jLink.size();j++){
                component_map[jLink[i][j]] = 0;
                if ( inverse_component_map.find(jLink[i][j]) == inverse_component_map.end() ) {
                    inverse_component_map[jLink[i][j]] = std::vector<std::pair<std::size_t,std::size_t> >();
                }
                inverse_component_map[jLink[i][j]].push_back(std::pair<std::size_t,std::size_t>(i,j));
            }
        }
        /*****************************************************************/

        /* Objects used to save the Hybridation functions to file */
        json jNextHyb;

        if(iteration) {
            /**********************************/
            /* We get the measurement results */
            json jMeas = jInputFile["Measurements"];
            /* And apply the sign */
            divideAllBy(jMeas,"Sign");
            /**********************************/

            std::string hybFileName = jParams["HYB"];
            json jHyb;
            IO::readJsonFile(outputFolder + hybFileName,jHyb);
            Hyb::accountForDifferentBeta(jHyb,beta);

            //We have to read all the Hyb components into variables so take them from the jLink variable
            
            std::size_t NHyb = 0;
            std::size_t NGreen = 0;
            for (auto &p : component_map)
            {
                if(p.first != "empty"){ //We don't read the empty component
                    if(NHyb == 0){
                        NHyb = jHyb[p.first]["real"].size();
                        NGreen = jMeas["GreenI_" + p.first].size();
                    }else if(NHyb != jHyb[p.first]["real"].size()){
                        throw std::runtime_error(p.first + ": missmatch in entry length's of the hybridisation function.");
                    }else if(NGreen != jMeas["GreenI_" + p.first].size()){
                        throw std::runtime_error(p.first + ": missmatch in entry length's of the measured Green's function function.");
                    }
                }
            } 
            
            hyb.resize(NGreen); 
            /************************************************/
            /**** We initialize the Hybridization object from data.  ******/
            for(std::size_t n = 0; n < std::min(NHyb, NGreen); ++n) {
                //We iterate over all components and read them from the Hyb file
                for (auto &p : component_map)
                {
                    if(p.first != "empty"){ //We don't read the empty component
                        p.second = std::complex<double>(jHyb[p.first]["real"][n],jHyb[p.first]["imag"][n]);
                    }
                } 
                //We initialize the hybridization matrix according to the Link file.
                IO::component_map_to_matrix(jLink,hyb[n],component_map);
            }
            /*** End initialisation from Matsubara data *****/
            /************************************************/
            /**************************************************************************************/
            /**** We initialize the Hyb object if the size doesn't match the Green's functions ****/
            /*** We use only the first moment expansion of the Hybridization for those components (this is usually a good starting point for the cycle) ***/
            for (auto &p : component_map)
            {
                if(p.first != "empty"){ //We don't read the empty component
                    if(!exists(jHyb[p.first],"First Moment")){
                        jHyb[p.first]["First Moment"]=0;
                    }
                    p.second = jHyb[p.first]["First Moment"];
                }
            } 
            for(std::size_t n = std::min(NHyb, NGreen); n < NGreen; ++n) {
                std::complex<double> iomega(.0, M_PI*(2*n + 1)/beta);
                std::map<std::string,std::complex<double> > component_map_divided_by_iomega;
                for (auto &p : component_map)
                {
                    if(p.first != "empty"){ //We don't read the empty component
                        component_map_divided_by_iomega[p.first] = p.second/iomega;
                    }
                } 
                IO::component_map_to_matrix(jLink,hyb[n],component_map_divided_by_iomega);
            }
            /*** End initialisation from Moments *****/
            /*****************************************/
            /********************************************************************************/
            /* We intialize the cluster Green's function from the Impurity Solver solution **/
            std::vector<RCuMatrix> green(NGreen);
            for(std::size_t n = 0; n < NGreen; ++n) {
                for (auto &p : component_map)
                {
                    if(p.first != "empty"){ //We don't read the empty component
                        p.second = std::complex<double>(jMeas["GreenR_" + p.first][n],jMeas["GreenI_" + p.first][n]);
                    }
                } 
                IO::component_map_to_matrix(jLink,green[n],component_map);
            }
            /* End Initialization of the cluster Green's function */
            /******************************************************/
            /*****************************/
            /* We compute the selfEnergy */
            for(std::size_t n = 0; n < NGreen; ++n) {
                std::complex<double> iomega(.0, (2*n + 1)*M_PI/beta);
                
                RCuMatrix temp;
                for(std::size_t i = 0;i<nSite_;i++){
                    temp(i,i) = iomega + mu;
                    temp(i + nSite_,i + nSite_) = -std::conj(iomega + mu);
                }

                temp -= hyb[n];     
                temp -= green[n].inv();
                
                selfEnergy.push_back(temp);
            }
            /* End compute selfEnergy */
            /**************************/
            /**************************************/
            /* We read the observables into files */

            if(exists(jParams,"n")){
                double const S = jParams["S"];
                double const current_N = jMeas["N"][0];
                double const target_N = jParams["n"];
                jParams["mu"] = mu - S*(current_N - target_N);
            }

            
            {
                std::ofstream file(dataFolder + "sign.dat", std::ios_base::out | std::ios_base::app);
                file << iteration << " " << jMeas["Sign"][0] << std::endl;
                file.close();
            }
            
            readScalSites("N", jMeas, iteration,dataFolder);
            readScalSites("k", jMeas, iteration,dataFolder);
            readScalSites("Sz", jMeas, iteration,dataFolder);
            readScalSites("D", jMeas, iteration,dataFolder);
            readScalSites("Chi0", jMeas, iteration,dataFolder);
                        
            {
                
                std::stringstream name; name << dataFolder << "pK" << iteration << ".dat";
                std::ofstream file(name.str().c_str(), std::ios_base::out);
                
    

                for(unsigned int k = 0; k < jMeas["pK"].size(); ++k) 
                    file << k << " " << jMeas["pK"][k] << std::endl;
                
                file.close();
            }
            
            if(jParams["EObs"] > .0) {
                
                {
                    std::stringstream name; name << dataFolder << "ChiFullSites" << iteration << ".dat";
                    std::ofstream file(name.str().c_str());
                    
                    for(unsigned int n = 0; n < jMeas["Chi"].size(); ++n) {
                        file << 2*n*M_PI/beta;
                        for(int i = 0; i < 4; ++i) {
                            std::string s = std::to_string(i);
                            file << " " << jMeas["Chi_" + s][n];
                        }
                        file << std::endl;
                    }
                    
                    file.close();
                }
                
                {
                    std::stringstream name; name << dataFolder << "ChiFull" << iteration << ".dat";
                    std::ofstream file(name.str().c_str());
                
                    for(unsigned int n = 0; n < jMeas["Chi"].size(); ++n){
                            file << 2*n*M_PI/beta << " " << jMeas["Chi"][n] << std::endl;
                    }
                    file.close();
                }               
            };
            /* End Reading observables */
            /***************************/

            /*************************************************/
            /* We copy the first moment to the next Hyb file */
            for (auto &p : inverse_component_map)
            {
                if(p.first != "empty"){
                    jNextHyb[p.first]["First Moment"] = jHyb[p.first]["First Moment"];
                }
            }
            /************************************************/


        } else {
            /******************************************************************************************************/
            /* Initialization of the self-energy using a self0.dat file or an the initialize_self_energy function */
            /* We initialize the simulation using a self file or an empty self-energy */
            double const EGreen = jParams["EGreen"];
            unsigned int const NSelf = beta*EGreen/(2*M_PI) + 1;
            
            json jSelf;
            std::ifstream selfFile(dataFolder + "self0.json"); 
            if(selfFile.good()) {
                selfFile >> jSelf;
                Hyb::accountForDifferentBeta(jSelf,beta);
            }

            std::string dummy;
            selfEnergy.resize(NSelf);
            for(std::size_t n = 0; n < NSelf; ++n) {
                /*******************************************/
                /* We try loading the selfEnergy file */
                if(selfFile.good())  
                {
                    for (auto &p : component_map)
                    {
                        if(p.first != "empty"){ //We don't read the empty component
                            p.second = std::complex<double>(jSelf[p.first]["real"][n],jSelf[p.first]["imag"][n]);
                        }
                    } 
                }else{
                    /*******************************************************************/
                    /* If this does not work, we initialize using this custom function */
                    initial_self_energy(jParams,n,component_map);
                    /*******************************************************************/
                }
                /*******************************************/
                IO::component_map_to_matrix(jLink,selfEnergy[n],component_map);
            }
            
            hyb.resize(selfEnergy.size());
            initial_Hyb_moments(jNextHyb,jParams);
            w = .0;         
        }
        /* Objects to save the self-energy and Green functions to file */
        json jSelf;
        json jGreen;
        w = std::complex<double>(jParams["weightR"],jParams["weightI"]);

        /************************************************************************************************/
        /* Now we compute the next cluster Green's function and from that, the next hybridation function */
        Int::EulerMaclaurin2D<RCuMatrix> integrator(1.e-10, 4, 12);
        for(std::size_t n = 0; n < selfEnergy.size(); ++n) {
            std::complex<double> iomega(.0, (2*n + 1)*M_PI/beta);
        
            addMatsubaraDataToJson(jSelf,inverse_component_map,selfEnergy[n]);
            
            RCuLatticeGreen latticeGreenRCu(iomega + mu, tpd, tpp, tppp, ep, selfEnergy[n]); 
            RCuMatrix greenNext = integrator(latticeGreenRCu, M_PI/2., M_PI/2.);

            addMatsubaraDataToJson(jGreen,inverse_component_map,greenNext);
            
            RCuMatrix hybNext;
            for(std::size_t i = 0;i<nSite_;i++){
                hybNext(i,i) = iomega + mu;
                hybNext(i + nSite_,i + nSite_) = -std::conj(iomega + mu);
            }
            hybNext -= selfEnergy[n];
            hybNext -= greenNext.inv();
            /***********************************************************************************/
            /* Finally we save the data for the output Hybi.json file.                         */
            /* We correct the new Hyb with the old Hyb to ensure convergence stability using w */
            RCuMatrix hybNextToWrite = (1. - w)*hybNext;
            hybNextToWrite+=w*hyb[n];
            addMatsubaraDataToJson(jNextHyb,inverse_component_map,hybNextToWrite);
            /**********************************************************/
        }
        /************************************************************************************************/
        /*********************************************************************/
        /*** Now we impose conditions on some of the the Hyb components  *****/
        
        for (auto& el : jNextHyb.items()){
            hyb_constraints(el.key(),el.value());
        }
        /************************************************************************/
        /**************************************/
        /* We save the self and green objects */
        IO::writeMatsubaraToJsonFile(dataFolder + "self" + std::to_string(iteration) + ".json",jSelf,beta);
        IO::writeMatsubaraToJsonFile(dataFolder + "green" + std::to_string(iteration) + ".json",jGreen,beta);
        /**************************************/

                
        /*******************************************************/
        /* We get ready for the next impurity Solver iteration */
        std::string const nextHybFileName = "Hyb" + std::to_string(iteration + 1) + ".json";
        jParams["HYB"] = nextHybFileName;
        
        IO::writeMatsubaraToJsonFile(outputFolder + nextHybFileName,jNextHyb,beta);
        
		IO::writeJsonToFile(outputFolder + "params" +  std::to_string(iteration + 1) + ".json",jParams);
        /*******************************************************/
    }
    catch(std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return -1;
    }
    catch(...) {
        std::cerr << "Fatal Error: Unknown Exception!\n";
        return -2;
    }
    return 0;
}









