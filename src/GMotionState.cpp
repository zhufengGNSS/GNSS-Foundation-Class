
//============================================================================
//
//  This file is part of GFC, the GNSS FOUNDATION CLASS.
//
//  The GFC is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation; either version 3.0 of the License, or
//  any later version.
//
//  The GFC is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with GFC; if not, write to the Free Software Foundation,
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
//
//  Copyright 2015, lizhen
//
//============================================================================

//
//  GMotionState.cpp
//  GFC
//
//  Created by lizhen on 05/06/2016.
//  Copyright © 2016 lizhen. All rights reserved.
//

#include "GMotionState.hpp"

namespace gfc
{
    
    GMotionState::GMotionState()
    {
        phiMatrix.resize(6, 6);
        phiMatrix(0,0) = 1.0;
        phiMatrix(1,1) = 1.0;
        phiMatrix(2,2) = 1.0;
        phiMatrix(3,3) = 1.0;
        phiMatrix(4,4) = 1.0;
        phiMatrix(5,5) = 1.0;
        
        dis_sat = 0.0;    // the distance from the sat to earth center
        dis_sat_sun = 0.0;  // in km
        eps = 0.0;  // the earth-probe-sun angle in radian
        shadow_factor = 1.0;
    }
    
    void GMotionState::updateState_ecef( GTime epoch_utc, GVector psatpos_ecef, GVector psatvel_ecef )
    {
        if( m_epoch != epoch_utc )
        {
            double PI = GCONST("PI");
            
            m_epoch = epoch_utc;
            
            JDTime jdt =  GTime::GTime2JDTime(m_epoch);
            
            ct = GTime::JDTime2CivilTime(jdt);
            
            satpos_ecef = psatpos_ecef;
            
            satvel_ecef = psatvel_ecef;
            
            GSpaceEnv::eop.ECEF2ECI_pos(satpos_ecef, satpos_eci);
            
            GSpaceEnv::eop.ECEF2ECI_vel(satpos_ecef, satvel_ecef, satvel_eci);
            
            keplerianElement.PV2KP(satpos_eci, satvel_eci);
            
            dis_sat = satpos_eci.norm();
            
            satposHat_eci = satpos_eci / dis_sat;
            
            satposHat_ecef = satpos_ecef/ dis_sat;
            
            satvelHat_eci = normalise(satvel_eci);
            
            satvelHat_ecef = normalise(satvel_ecef);
            
            sat_sun_eci = GSpaceEnv::planetPos_eci[GJPLEPH::SUN] - satpos_eci ;
            
            dis_sat_sun = sat_sun_eci.norm();
            
            sat_sunHat_eci = sat_sun_eci/dis_sat_sun;
            
            sat_sun_ecef = GSpaceEnv::planetPos_ecef[GJPLEPH::SUN] - satpos_ecef;
            
            sat_sunHat_ecef = sat_sun_ecef/ dis_sat_sun;
            
            //GVector mysunpos_eci = GSpaceEnv::planetPos_eci[GJPLEPH::SUN];
            
//            if(!strcmp("2007/01/20/01:27:32.000000", GTime::GTime2CivilTime(epoch_utc).TimeString().c_str()))
//            {
//                int testc = 0;
//            }
            
            // get the eps angle
            eps = acos( dotproduct( sat_sun_eci , -satpos_eci)/dis_sat/dis_sat_sun );
            GVector sunpos_u_eci = normalise( GSpaceEnv::planetPos_eci[GJPLEPH::SUN] );
            
            // myshadowFactor has bugs
            shadow_factor = myshadowFactor(GSpaceEnv::planetPos_ecef[GJPLEPH::SUN], satpos_ecef);
            //shadow_factor = GSpaceCraftAttitude::eclipse( satpos_eci,GSpaceEnv::planetPos_eci[GJPLEPH::SUN] );
            
            //shadow_factor = shadowFactor_SECM(true,GSpaceEnv::planetPos_ecef[GJPLEPH::SUN], satpos_ecef);
            
            //shadow_factor = shadowFactor(<#double a#>, <#double b#>, GSpaceEnv::planetPos_eci[GJPLEPH::SUN], satpos_eci);
            
            printf("%s : %f\n",GTime::GTime2CivilTime(epoch_utc).TimeString().c_str(), shadow_factor);
            if(shadow_factor < 0.0 || shadow_factor > 1.0 || isnan(shadow_factor))
            {
                printf("%s: %f \n",GTime::GTime2CivilTime(epoch_utc).TimeString().c_str(), shadow_factor);
                int testc = 0;
            }

            
            //printf("%.6f %.6f %.6f %.6f %.6f %.6f\n",satpos_ecef.x,satpos_ecef.y,satpos_ecef.z,
            //       GSpaceEnv::planetPos_ecef[GJPLEPH::SUN].x,
            //       GSpaceEnv::planetPos_ecef[GJPLEPH::SUN].y,
            //       GSpaceEnv::planetPos_ecef[GJPLEPH::SUN].z);

             dis_factor = (GCONST("AU")/dis_sat_sun)*(GCONST("AU")/dis_sat_sun);
            
            //update attitude
            attitude_eci.updateAttitude(satpos_eci,GSpaceEnv::planetPos_eci[GJPLEPH::SUN] , sat_sun_eci, satvel_eci,shadow_factor);
            
            // convert attitude_eci to attitude_ecef
            attitude_ecef = attitude_eci;
            attitude_ecef.convert2ECEF(GSpaceEnv::eop);
            
            
            //calculate the orbit ascending node vector
            orbit_node_vector_eci = crossproduct(GVector(0,0,1.0), attitude_eci.n);
            orbit_node_vector_eci.normalise();
            double cosu = dotproduct(orbit_node_vector_eci, satposHat_eci);
            
            GVector tmpA = crossproduct(orbit_node_vector_eci, satposHat_eci);
            double sinu = tmpA.norm();
            
            //u = atan2(sinu, cosu);
            //u = keplerianElement.m_argp + keplerianElement.m_tran;
            
            //update attitude
//            if( earthFlux.m_flux.size() > 0 )
//            {
//                earthFlux.m_flux.clear();
//            }
            
            //clock_t start, end;
            //start = clock();
            
            //update the flux value, which will use the eclipse state  of the satellite
            earthFlux.makeFlux( ct.m_month - 1, GSpaceEnv::planetPos_ecef[GJPLEPH::SUN] , satpos_ecef, dis_sat );
            
            //end = clock();
            
            //double time = double(end - start)/CLOCKS_PER_SEC;
            
            //update the flux value, which will use the eclipse state  of the satellite, with eci
            //earthFlux.makeFlux( ct.m_month - 1, GSpaceEnv::planetPos_eci[GJPLEPH::SUN] , satpos_eci, dis_sat );
            
            solarFlux.makeFlux( GSpaceEnv::tsi*shadow_factor*dis_factor, sat_sunHat_eci, dis_sat_sun);
            
        }
        
    }
    
    /*
     *
     * update the state
     */
    void GMotionState::updateState_eci(GTime epoch_utc, GVector psatpos_eci, GVector psatvel_eci )
    {
        
        if( epoch_utc != m_epoch )
        {
            double PI = GCONST("PI");
            
            m_epoch = epoch_utc;
            
            JDTime jdt =  GTime::GTime2JDTime(m_epoch);
            
            ct = GTime::JDTime2CivilTime(jdt);
            
            satpos_eci = psatpos_eci;
            satvel_eci = psatvel_eci;
            
            keplerianElement.PV2KP(satpos_eci, satvel_eci);
            
            GSpaceEnv::eop.ECI2ECEF_pos(satpos_eci, satpos_ecef);
            GSpaceEnv::eop.ECI2ECEF_vel(satpos_eci, satvel_eci, satvel_ecef);
            
            dis_sat = satpos_eci.norm();
            
            satposHat_eci = satpos_eci / dis_sat;
            
            satposHat_ecef = satpos_ecef/ dis_sat;
            
            satvelHat_eci = normalise(satvel_eci);
            
            satvelHat_ecef = normalise(satvel_ecef);
            //sat_sun_eci is from sat to sun
            sat_sun_eci = GSpaceEnv::planetPos_eci[GJPLEPH::SUN] - satpos_eci ;
            dis_sat_sun = sat_sun_eci.norm();
            sat_sunHat_eci = sat_sun_eci/dis_sat_sun;
            
            sat_sun_ecef = GSpaceEnv::planetPos_ecef[GJPLEPH::SUN] - satpos_ecef;
            sat_sunHat_ecef = sat_sun_ecef/ dis_sat_sun;
            
            dis_factor = (GCONST("AU")/dis_sat_sun)*(GCONST("AU")/dis_sat_sun);
            
            //// myshadowFactor has bugs
            if(!strcmp(GTime::GTime2CivilTime(epoch_utc).TimeString().c_str(),"2015/01/09/21:29:18.166667") )
            {
                int tesc = 0;
            }
            
            shadow_factor = myshadowFactor(GSpaceEnv::planetPos_ecef[GJPLEPH::SUN], satpos_ecef);
            //shadow_factor = shadowFactor_SECM(false,GSpaceEnv::planetPos_ecef[GJPLEPH::SUN], satpos_ecef);
            //shadow_factor = GSpaceCraftAttitude::eclipse( satpos_eci,GSpaceEnv::planetPos_eci[GJPLEPH::SUN] );
            
            //printf("%s : %f \n",GTime::GTime2CivilTime(epoch_utc).TimeString().c_str(), shadow_factor);
            
            if(shadow_factor < 0.0 || shadow_factor > 1.0)
            {
                printf("%s: %f\n",GTime::GTime2CivilTime(epoch_utc).TimeString().c_str(), shadow_factor);
                int testc = 0;
            }
            
            //update attitude, always use ECI to update the attitude
            //shadow_facor = 1.0;
            attitude_eci.updateAttitude(satpos_eci,GSpaceEnv::planetPos_eci[GJPLEPH::SUN] , sat_sun_eci, satvel_eci,shadow_factor);
            
            attitude_ecef = attitude_eci;
            attitude_ecef.convert2ECEF(GSpaceEnv::eop);
            
            //attitude_ecef.updateAttitude(satpos_ecef, GSpaceEnv::planetPos_ecef[GJPLEPH::SUN], sat_sun_ecef, satvel_ecef,shadowfactor);
            
            //calculate the orbit ascending node vector
            GVector z_eci(0,0,1);
            orbit_node_vector_eci = crossproduct(z_eci, attitude_eci.n);
            orbit_node_vector_eci.normalise();
            double cosu = dotproduct(orbit_node_vector_eci, satposHat_eci);
            
            GVector tmpA = crossproduct(orbit_node_vector_eci, satposHat_eci);
            double sinu = tmpA.norm();
            
            //u = atan2( sinu, cosu );
            
            //u = keplerianElement.m_argp + keplerianElement.m_tran;
            
            // get the eps angle
            eps = acos( -dotproduct(sat_sun_eci, satpos_eci)/(dis_sat_sun*dis_sat) );
            
            //update attitude
            if( earthFlux.m_flux.size() > 0 )
            {
                earthFlux.m_flux.clear();
            }
            
            //update the flux value, which will use the eclipse state  of the satellite, with ecef, use ct.m_month
            earthFlux.makeFlux( ct.m_month - 1, GSpaceEnv::planetPos_ecef[GJPLEPH::SUN] , satpos_ecef, dis_sat );
            
            solarFlux.makeFlux( GSpaceEnv::tsi*shadow_factor*dis_factor, sat_sunHat_eci, dis_sat_sun);
            
        }
    }
    
    void GMotionState::updateTransitionMatrix(double *phi)
    {
        phiMatrix.setData(phi, 6, 6);
    }
    void GMotionState::updateTransitionMatrix(GMatrix phi)
    {
        phiMatrix = phi;
    }
    
    
    void GMotionState::updateSensitivityMatrix(double* S)
    {
        int np = senMatrix.getColNO();
        senMatrix.setData(S, 6, np);
    }
    
    void GMotionState::updateSensitivityMatrix(GMatrix S)
    {
        senMatrix = S;
    }
    
    GSpaceCraftAttitude* GMotionState::getAttitude()
    {
        return &attitude_eci;
    }
    
    GSpaceCraftAttitude* GMotionState::getAttitude_ecef()
    {
        GSpaceCraftAttitude atti_ecef;
        //has to be check in the future
        return  &attitude_eci;
    }
    
    void GMotionState::outputState( ofstream& file )
    {
        static bool first = true;
    }
    
    void GMotionState::shadow_detector(GTime gpst,gfc::GVector &XSun, gfc::GVector &XSat)
    {
        static double pre_factor = 1.0;
        static bool first  = true;
        bool static umbra_start = false;
        
        double myfactor = 1.0;
        if(first == true)
        {
            myfactor = GMotionState::myshadowFactor(XSun,XSat);
            //myfactor = GMotionState::shadowFactor_SECM(true, XSun, XSat);
            pre_factor = myfactor;
            first = false;
            return;
        }
   
        myfactor = GMotionState::myshadowFactor(XSun,XSat);
        //myfactor = GMotionState::shadowFactor_SECM(true, XSun, XSat);
        //printf("%f %f\n",myfactor1,myfactor);
        
        //GTime     gpst =  epoch_gps;
        
        double ct = gpst.toDays();
        //CivilTime ct =GTime::GTime2CivilTime(GTime::UTC2GPST(m_epoch));
        
        
        if(first == false)
        {
            //judge the eclipse event according to pre_facor and myfactor
            if(pre_factor == 1.0 && myfactor < 1.0)
            {
                umbra_start=true;
                //printf("eclipse_start: %16.10f\n", ct  );
                printf("eclipse_start: %16.10f %s\n", ct, GTime::GTime2CivilTime(gpst).TimeString().c_str());
                //printf("pre:%f -- cur:%f\n",pre_factor,myfactor);
            }
            else if(pre_factor > 0.0 && myfactor == 0.0)
            {
                umbra_start = false;
                //printf("umbra_s: %16.10f\n", ct);
                printf("umbra_e: %16.10f %s\n", ct, GTime::GTime2CivilTime(gpst).TimeString().c_str());
                //printf("pre:%f -- cur:%f\n",pre_factor,myfactor);
            }
            else if(pre_factor == 0.0 && myfactor > 0.0)
            {
                umbra_start= true;
                //printf("umbra_s: %16.10f\n", ct);
                printf("umbra_s: %16.10f %s\n", ct, GTime::GTime2CivilTime(gpst).TimeString().c_str());
                //printf("pre:%f -- cur:%f\n",pre_factor,myfactor);
            }
            else if(pre_factor < 1.0 && myfactor == 1.0)
            {
                umbra_start = false;
                printf("eclipse_end: %16.10f %s\n", ct, GTime::GTime2CivilTime(gpst).TimeString().c_str());
                //printf("pre:%f -- cur:%f\n",pre_factor,myfactor);
            }
            
            pre_factor = myfactor;
            if(umbra_start == true)
            {
                printf("%s: %f\n",GTime::GTime2CivilTime(GTime::UTC2GPST(m_epoch)).TimeString().c_str(), myfactor);
            }
           
        }
        
        
        
    }
    
    
    double GMotionState::shadowFactor_SECM(bool on, GVector& XSun, GVector& XSat)
    {
        
        double factor = 1.0;
        
        double rad_sun = 695700.0; //695700.0
        double rad_Earth= 6371.0;  //6371.0
        
        
        double p = 6378.137;  //6408.137
        double q = 6356.7523142;  //6386.65173
        
        //double p = 6378.137 + 50;  //6408.137
        //double q = 6356.7523142*(p/6378.137);  //6386.65173
        
        double q2 = q*q;
        double p2 = p*p;
        double q_p2 = q2/p2;
        
        
        GVector sat_sun = XSun - XSat;
        
        double xxyy = XSat.x * XSat.x + XSat.y * XSat.y;
        double zz = XSat.z * XSat.z;
        GVector n = crossproduct(XSat, sat_sun);
        
        if(on == true)
        {
            // If M is a matrix [[q,0,0],[0,q,0],[0,0,p]]
            // A = M * M / p^2
            // B = M / sqrt(p * q)
            double rTAr = xxyy * q_p2 + zz;
            double rTAs = (XSat.x * sat_sun.x + XSat.y * sat_sun.y) * q_p2 + (XSat.z * sat_sun.z);
            double Br_cross_Bs_sqr = (n.x * n.x + n.y * n.y) + (n.z * n.z) * q_p2;
            
            // The discriminant of a quadratic used to solve for the linear combination
            // of rso and sun vectors that give the Earth edge vector.
            double disc = std::sqrt(((xxyy - p2) * q2 + zz * p2) / Br_cross_Bs_sqr);
            
            GVector edge = ((q2 - rTAs * disc) / rTAr - 1.0) * XSat + disc * sat_sun;
            
            GVector rr = edge + XSat;
            //replace the radius of earth by the corrected rr.norm()
            rad_Earth = rr.norm();
        }
        
        GVector sat_sun_u = normalise(sat_sun);
        GVector xsat_u = normalise(XSat);
        
        double dis_sat_sun = sat_sun.norm();
        double s = XSat.norm();
        
        // https://en.wikipedia.org/wiki/Angular_diameter
        // angular radius
        double a = asin(rad_sun/dis_sat_sun);
        double b = asin(rad_Earth/s);
        double c = acos( -dotproduct(xsat_u, sat_sun_u)  );
        
        //printf("%f %f %f\n", a, b, c);
        
        if( a+b <= c )
        {
            factor = 1.0;
        }
        else if( c+a< b)
        {
            factor = 0.0;
        }
        else  // fabs(a-b)<c && c < a+b
        {
            double x = (c*c + a*a -b*b)/(2.0*c);
            double y = sqrt(a*a -x*x);
            double A = a*a*acos(x/a)+b*b*acos((c-x)/b)-c*y;
            factor = 1.0 - A/(M_PI*a*a);
        }
       
        
        
        
        return factor;
    }
    
    
    
    int GMotionState::myperspectiveProjection(double a, double b, GVector& sunpos_ecef, GVector& satpos_ecef, double& r_solar, double& area_bright, double& dis_boundary, double& dis_circle)
    {
        int state = -1;
        double Rs = 695700.0; //km
        double ab = a*b;
        double a2 = a*a;
        double b2 = b*b;
        double ab2 = ab*ab;
        
        GVector r = satpos_ecef;
        GVector rs = sunpos_ecef;
        
        double t =0.0, t1 = 0.0, t2 =0.0, dis =0, s1 = 0, s2=0,ds1 = 0.0, ds2 =0.0;
        //A: first test if the satellite is in the front of earth,
        // if it is in the front of earth, it is always full phase
        // otherwise,using the photogrammetry method
        // A= diag{1/a2,1/a2, 1/b2 }, A^{-1} = diag{a2, a2, b2}
        
        GVector o, u, v, n;
        double f = 1000.0;
        n = r - rs;
        double dis_sat_earth = r.norm();
        double dis_sat_sun = n.norm();
        n.normalise();
        
        double nAin = n.x*n.x*a2 + n.y*n.y*a2 + + n.z*n.z*b2;
        double rtn = dotproduct(r, n);
        double ntn = dotproduct(n, n);
        
        
        s1 = sqrt(1.0/nAin);
        s2 = - s1;
        
        t1 = (rtn - 1.0/s1)/ntn;
        t2 = (rtn - 1.0/s2)/ntn;
        
        //GVector xp1(s1*n.x/a2, s1*n.y/b2, s1*n.z/b2 );
        //GVector xp2(s2*n.x/a2, s2*n.y/b2, s2*n.z/b2 );
        
        GVector xs1 = r - t1*n;
        GVector xs2 = r - t2*n;
        
        ds1 = (xs1-rs).norm();
        ds2 = (xs2-rs).norm();
        
        
        t = (ds1 <= ds2) ? ds1:ds2;
        ds2 = (ds1 >= ds2)? ds1:ds2;
        ds1 = t;
        
        
       
        
        
        double nAn = n.x*n.x/a2 + n.y*n.y/a2 + n.z*n.z/b2;
        double rsAn = n.x*rs.x/a2 + n.y*rs.y/a2 + n.z*rs.z/b2;
        double rsArs = rs.x*rs.x/a2 + rs.y*rs.y/a2 + rs.z*rs.z/b2;
        double Delta = rsAn*rsAn - nAn*(rsArs-1.0);
        
        if(Delta > 0) // sun-sat line intersects the Earth ellipsoid
        {
            t1 = (-2.0*rsAn + sqrt(Delta))/2.0/nAn;
            t2 = (-2.0*rsAn - sqrt(Delta))/2.0/nAn;
            
            ds1 = (t1 <= t2) ? t1:t2;
            //ds2 = (t1 <= t2) ? t2:t1;
        }
        else
        {
            // normal vector to the plane sat,sun and Earth
            GVector p = crossproduct(r, n);
            // normal at the ellipsoid that is perpendicular to n
            GVector ns = crossproduct(n, p);
            ns.normalise();
            
            t = ns.x*a2*ns.x + ns.y*a2*ns.y + ns.z*b2*ns.z ;
            double lam1 = sqrt( 1.0/ t );
            double lam2 = -lam1;
            
            s1 = dotproduct(ns, rs) - lam1*t;
            s2 = dotproduct(ns, rs) - lam2*t;
            
            double lam = fabs(s1)<fabs(s2)? lam1: lam2;
            dis = lam*(n.x*ns.x*a2 + n.y*ns.y*a2 + n.z*ns.z*b2) - dotproduct(n, rs);
            
            ds1 = dis;
        }
         // sun-sat line DOES NOT intersect the Earth ellipsoid, get the tangents
        
        if(dis_sat_sun < ds1)  // full phase
        {
            state = 0;
            //导致后面的 area_bright, dis_boundary, and dis_circle 都没有计算
            
            return state;
        }
        else if(dis_sat_sun >= ds2)  // ellipse
        {
            state = 2;
        }
        else if( dis_sat_sun >= ds1 && dis_sat_sun <= ds2 ) // hyperbola
        {
             state = 1;  // hyperbola
        }
        
        //B: build the ISF coordinate system
        o = r - f*n; // the origin of the photo coordinate system (PSC)
        
        u = n - 1.0/rtn*r; //只有这个定义没问题
        
        u.normalise();
        v = crossproduct(n, u);
        
        // the projection of earth's centre PEC in ECEF
        double t_dis = sqrt( (f*dis_sat_earth/rtn)*(f*dis_sat_earth/rtn) - f*f );
        GVector PEC = t_dis*u; // vector from PSC to PEC
        GVector PEC_isf;
        // convert PEC into ISF
        PEC_isf.x = dotproduct(u, PEC);
        PEC_isf.y = dotproduct(v, PEC);
        PEC_isf.z = dotproduct(n, PEC);
        
        t = (r.x*r.x/a2 + r.y*r.y/a2 + r.z*r.z/b2 - 1.0);
        
        double M[3][3]={0.0};
        M[0][0] = r.x*r.x/a2/a2 - t/a2;
        M[0][1] = r.x*r.y/a2/a2;
        M[0][2] = r.x*r.z/a2/b2;
        M[1][0] = M[0][1];
        M[1][1] = r.y*r.y/a2/a2 - t/a2;
        M[1][2] = r.y*r.z/a2/b2;
        M[2][0] = M[0][2];
        M[2][1] = M[1][2];
        M[2][2] = r.z*r.z/b2/b2 - t/b2;
        
        
        //D: calculate K
        double K[6] = {0.0};
        
        K[0] = (u.x*M[0][0] + u.y*M[1][0] + u.z*M[2][0])*u.x
             + (u.x*M[0][1] + u.y*M[1][1] + u.z*M[2][1])*u.y
             + (u.x*M[0][2] + u.y*M[1][2] + u.z*M[2][2])*u.z;
        
        K[1] = (u.x*M[0][0] + u.y*M[1][0] + u.z*M[2][0])*v.x
            + (u.x*M[0][1] + u.y*M[1][1] + u.z*M[2][1])*v.y
            + (u.x*M[0][2] + u.y*M[1][2] + u.z*M[2][2])*v.z;
        
        K[2] = (v.x*M[0][0] + v.y*M[1][0] + v.z*M[2][0])*v.x
            + (v.x*M[0][1] + v.y*M[1][1] + v.z*M[2][1])*v.y
            + (v.x*M[0][2] + v.y*M[1][2] + v.z*M[2][2])*v.z;
        
        K[3] = -((u.x*M[0][0] + u.y*M[1][0] + u.z*M[2][0])*n.x
            + (u.x*M[0][1] + u.y*M[1][1] + u.z*M[2][1])*n.y
            + (u.x*M[0][2] + u.y*M[1][2] + u.z*M[2][2])*n.z)*f*2.0;
        
        K[4] = -((v.x*M[0][0] + v.y*M[1][0] + v.z*M[2][0])*n.x
            + (v.x*M[0][1] + v.y*M[1][1] + v.z*M[2][1])*n.y
            + (v.x*M[0][2] + v.y*M[1][2] + v.z*M[2][2])*n.z)*f*2.0;
        
        K[5] = ((n.x*M[0][0] + n.y*M[1][0] + n.z*M[2][0])*n.x
            + (n.x*M[0][1] + n.y*M[1][1] + n.z*M[2][1])*n.y
            + (n.x*M[0][2] + n.y*M[1][2] + n.z*M[2][2])*n.z)*f*f;
        
        
        //printf("%.12f %.12f %.12f %.12f %.12f %.12f\n",K[0]-KK[0],K[1]-KK[1],K[2]-KK[2],K[3]-KK[3],K[4]-KK[4],K[5]-KK[5]);
      
        
        
        // just make the numbers larger for visualisation
        for( int i =0; i< 6; i++ )
        {
            K[i] = K[i]*1.0E6;
        }
        
        //E: calculate the eigen value and eigen vector of matrix B
        t =  sqrt( (K[0]+K[2])*(K[0]+K[2]) - 4.0*(K[0]*K[2] - K[1]*K[1]) ) ;
        double lambda1 = (K[0]+K[2]+t)/2.0;
        double lambda2 = (K[0]+K[2]-t)/2.0;
        
        double  r1[2]={0.0,1.0},r2[2]={1.0,0.0};
        // http://www.math.harvard.edu/archive/21b_fall_04/exhibits/2dmatrices/index.html
        
        
        if( fabs(K[1]) < 1.0E-12)
        {
            r1[0] = 1;
            r1[1] = 0;

            r2[0] = 0;
            r2[1] = 1;
        }
        else
        {
            r1[0] = lambda1 - K[2];
            r1[1] = K[1];
            r2[0] = lambda2 - K[2];
            r2[1] = K[1];
        }
    
        //get the unit vector
        t = sqrt(r1[0]*r1[0]+r1[1]*r1[1]);
        r1[0] = r1[0]/t;r1[1] = r1[1]/t;
        
        t= sqrt(r2[0]*r2[0]+r2[1]*r2[1]);
        r2[0] = r2[0]/t;r2[1] = r2[1]/t;
        
//        //the larger eigen value is lambda1
        if( fabs(lambda1) <= fabs(lambda2) )  // swap lamda1 and lamda2, with r1 and r2
        {
            t = lambda1;
            lambda1 = lambda2;
            lambda2 = t;
            double r[2] ={r1[0],r1[1]};
            r1[0] = r2[0]; r1[1] = r2[1];
            r2[0] = r[0]; r2[1] = r[1];
        }
        
        double OM = (K[2]*K[3]*K[3] - 2.0*K[1]*K[3]*K[4] + K[0]*K[4]*K[4] )/(4.0*(K[0]*K[2] - K[1]*K[1]));
        // the translation parameters
        double tx = (r1[0]*K[3] + r1[1]*K[4])/lambda1;
        double ty = (r2[0]*K[3] + r2[1]*K[4])/lambda2;
        
        // semi-major axis and the semi-minor axis
        double AA =  ( OM - K[5] )/lambda1;
        double BB =  ( OM - K[5] )/lambda2;
        double R0 = f*Rs/dis_sat_sun;
        r_solar = R0;
        
        //F: Solve the quartic in the translated and rotated frame
        double A = lambda1*(R0 - 0.5*tx )*(R0 - 0.5*tx) + 0.25*lambda2*ty*ty - OM + K[5];
        double B = 2.0*lambda2*R0*ty;
        double C = lambda1*(0.5*tx*tx - 2*R0*R0) + lambda2*(0.5*ty*ty + 4*R0*R0) - 2*OM + 2*K[5];
        double D = 2*lambda2*R0*ty;
        double E = lambda1*(R0+0.5*tx)*(R0+0.5*tx) + 0.25*lambda2*ty*ty - OM + K[5];
        
        double ce[4]={B/A,C/A,D/A,E/A};
        
        double XX[4]={0.0}; // the solution of quartic equation
        int num_of_solution = 0;
        
        num_of_solution = GMath::solve_quartic(ce[0], ce[1], ce[2], ce[3], XX);
        
        if( num_of_solution == 3 || num_of_solution == 4)
        {
            printf("WARNING: shadowfactor, %d intersections!\n", num_of_solution);
            exit(0);
        }
        
        //calculate the sun's projection centre Os (PSC) in the transformed frame
        double Os[2] = {0.5*tx , 0.5*ty };
        //double Os[2] = {0.0,0.0};
        double PEC_new[2] = {0.0,0.0};
        // the projection of the earth's centre Pe in the transformed frame is obtained by converting PEC_isf into the rotated and translated frame
        PEC_new[0] = PEC_isf.x*r1[0] + r1[1]*PEC_isf.y + 0.5*tx;
        PEC_new[1] = PEC_isf.x*r2[0] + r2[1]*PEC_isf.y + 0.5*ty;
        
        //printf("PEC_new: %f %f\n",PEC_new[0], PEC_new[1]);
        
        // calculate the coordinates of the intersections betweent the circle and the conical curve in transformed frame
        // in the new frame, the origin is at the centre of the projection of the Earth
        double Q1[2]={ (1.0 - XX[0]*XX[0])/(1.0 + XX[0]*XX[0])*R0 + 0.5*tx , 2*XX[0]/(1.0 + XX[0]*XX[0])*R0 + 0.5*ty};
        double Q2[2]={ (1.0 - XX[1]*XX[1])/(1.0 + XX[1]*XX[1])*R0 + 0.5*tx , 2*XX[1]/(1.0 + XX[1]*XX[1])*R0 + 0.5*ty};
        
        
        //PEC_new[0] = (PEC_isf.x-0.5*tx)*r1[0] + (PEC_isf.y-0.5*ty)*r2[0];
        //PEC_new[1] = (PEC_isf.x-0.5*tx)*r1[1] + (PEC_isf.y-0.5*ty)*r2[1];
        
        
        //double boundary_intersection[2] ={0.0};
        //PEC_new[0] = 0.0;
        //PEC_new[1] = 0.0;
        
        //double mytest = (lambda1*Os[0]*Os[0] + lambda2*Os[1]*Os[1] )/(OM-K[5]);
        
        //figure out the intersection between the line from PEC_new to Os and the conical curve.
        // parameter equation of line from Oe to Os in transformed frame, x = td
        double PEC_Os_len = sqrt( (Os[0]-PEC_new[0])*(Os[0]-PEC_new[0]) + (Os[1]-PEC_new[1])*(Os[1]-PEC_new[1]));
        double d[2] = { (Os[0]-PEC_new[0])/PEC_Os_len,(Os[1]-PEC_new[1])/PEC_Os_len };
        double aa = lambda1*d[0]*d[0] + lambda2*d[1]*d[1];
        double bb = 2.0*(lambda1*PEC_new[0]*d[0] + lambda2*PEC_new[1]*d[1]);
        double cc = lambda1*PEC_new[0]*PEC_new[0] + lambda2*PEC_new[1]*PEC_new[1] + K[5] - OM;
        t1 = (-bb + sqrt(bb*bb - 4.0*aa*cc ))/aa/2.0;
        t2 = (-bb - sqrt(bb*bb - 4.0*aa*cc ))/aa/2.0;
        
        if(t1*t2<0.0)
        {
            t = t1>0?t1:t2;
        }
        else
        {
           t =  t1<=t2?t1:t2;
        }
        
        dis_boundary = t;
        
        //boundary_intersection[0] = PEC_new[0] + t*d[0];
        //boundary_intersection[1] = PEC_new[1] + t*d[1];
        
        // //figure out the intersection between the line from PEC_new to Os and the solar circle
        aa = d[0]*d[0] + d[1]*d[1];
        bb = 2*(PEC_new[0]*d[0]+PEC_new[1]*d[1]) - (d[0]*tx + d[1]*ty );
        cc = PEC_new[0]*PEC_new[0]+PEC_new[1]*PEC_new[1] - (PEC_new[0]*tx + PEC_new[1]*ty ) + 0.25*(tx*tx+ty*ty) - R0*R0;
        
        t1 = (-bb + sqrt(bb*bb - 4.0*aa*cc ))/aa/2.0;
        t2 = (-bb - sqrt(bb*bb - 4.0*aa*cc ))/aa/2.0;
        // t should be the smaller one, which means closer to the solid Earth
        if(t1*t2<0.0)
        {
            t = t1>0?t1:t2;
        }
        else
        {
            t =  t1<=t2?t1:t2;
        }
        
        dis_circle = t;
        
        //sun_intersection[0] = PEC_new[0] + t*d[0];;
        //sun_intersection[1] = PEC_new[1] + t*d[1];;
        
        
        t = K[0]*K[2] - K[1]*K[1];
        if( t > 0.0) // ellipse
        {
            bool in_out = false;
            
            if( OM/(OM-K[5]) <= 1.0 )
            {
                in_out = true;
            }
            else
            {
                in_out = false;
            }
            
            if( in_out == true && num_of_solution < 2 )  //umbra
            {
                state = -1;
                return state;
            }
            if( in_out == false && num_of_solution < 2 )  //full phase
            {
                state = 0;
                return state;
            }
            
            // penumbra
            area_bright = area_ellispe(Q1, Q2, Os, R0, sqrt(AA), sqrt(BB), in_out);
            
        }
        else if (t < 0.0) // hyperbola
        {
            bool in_out = false;
            
            if( OM/(OM-K[5]) <= 1.0 )
            {
                in_out = false;
            }
            else
            {
                in_out = true;
            }
            
            if( in_out == true && num_of_solution <2)
            {
                state = -1; // umbra
                return state;
            }
            
            if( in_out == false && num_of_solution <2)
            {
                state = 0; // full phase
                return state;
            }
            
            // penumbra
            double a =0, b =0;
            bool x_axis = true;
            if(AA > 0)
            {
                a =sqrt(AA);
            }
            else
            {
                a = sqrt(-AA);
                x_axis = false;
            }
            if(BB > 0)
            {
                b =sqrt(BB);
                x_axis =false;
            }
            else
            {
                b = sqrt(-BB);
            }
            
            area_bright = area_hyperbola( Q1, Q2, Os,PEC_new, R0, a, b, in_out, x_axis);
            
            // similar as that in the ellipse case
        }
        
        return state;
    }
    
    
    double GMotionState::area_hyperbola(double Q1[2], double Q2[2], double Os[2],double Oe[2], double Rs,double a, double b, bool in_out, bool x_axis)
    {
        double area = 0.0;
        double as[2] = {Q1[0]-Os[0],Q1[1] -Os[1] };
        double bs[2] = {Q2[0]-Os[0],Q2[1] -Os[1] };
        
        double ae[2] = {Q1[0]-Oe[0],Q1[1] -Oe[1]  };
        double be[2] = {Q2[0]-Oe[0],Q2[1] -Oe[1]  };
        
        
        double TQ1Q2Os = 0.5*fabs(as[0]*bs[1] - bs[0]*as[1]);
        double TQ1Q2Oe = 0.5*fabs(ae[0]*be[1] - be[0]*ae[1]);
        
        double cts = (as[0]*bs[0] + as[1]*bs[1] )/sqrt( (as[0]*as[0]+as[1]*as[1]) * (bs[0]*bs[0]+bs[1]*bs[1]) );
        double SQ1Q2Os = 0.5 * acos(cts)*Rs*Rs;
        
        //calculate the area of hyperbolic secttion
        
        
        //double SQ1Q2Oe = 0.5*a*b*fabs( acos(ae[0]/a) - acos(be[0]/a) );
        //double S1 = SQ1Q2Oe - TQ1Q2Oe;
        
        if(x_axis == false) //// 主轴是y, 整个双曲线旋转90度，使得x为主轴
        {
            // rotation transformation
            // x' = y
            // y' =-x
            double tmp = 0.0;
            tmp = Q1[0]; Q1[0] = Q1[1]; Q1[1] = -tmp;
            tmp = Q2[0]; Q2[0] = Q2[1]; Q2[1] = -tmp;
            tmp = a;
            a = b;
            b = tmp;
        }
        
        //ref: http://www.robertobigoni.eu/Matematica/Conics/segmentHyp/segmentHyp.html
        double s1 = 0.0, s2 =0.0;
        
        double xQ1 = fabs(Q1[0]);
        double xQ2 = fabs(Q2[0]);
        
        s1 = b*( xQ1*sqrt(  (xQ1*xQ1/a/a) -1.0 ) - a*acosh(xQ1/a) );
        
        s2 = b*( xQ2*sqrt(  (xQ2*xQ2/a/a) -1.0 ) - a*acosh(xQ2/a) );
        
        if(isnan(s1) || isnan(s2) )
        {
            int testc = 0;
        }
        
        double ss = s1<=s2?s1:s2;
        double sl = s1>=s2?s1:s2;
        double S2 =0.0;
        if( Q1[1]*Q2[1] < 0.0 )  //2个交点在y轴不同侧
        {
            double x_m = 0.0;
            
            if(fabs(Q2[0] - Q1[0])>1.0E-10)
            {
                double k = ( Q2[1] - Q1[1] )/( Q2[0] - Q1[0]);
                double m = Q1[1] - k *Q1[0];
                x_m = - m/k;
            }
            else
            {
                x_m = Q1[0];
                //S2 = sl;
            }
            
            if(Q1[0]>Q2[0])
            {
                S2 = ss + (sl - ss)/2.0 - 0.5*fabs((x_m - Q1[0])*Q1[1]) + 0.5*fabs((Q2[0]- x_m)*Q2[1]) ;
            }
            else
            {
                S2 = ss + (sl - ss)/2.0 - 0.5*fabs((Q2[0]- x_m)*Q2[1]) + 0.5*fabs((x_m - Q1[0])*Q1[1]);
            }
            
            
        }
        else  //2 个交点在同一侧
        {
            double s_trapizium = 0.5*fabs( Q1[0] - Q2[0] )* (fabs(Q1[1]) + fabs(Q2[1]) );
            S2 = (sl - ss - 2*s_trapizium )/2.0;
        }
        
        
        if(in_out == true) // the centre of the sun is inside the ellipse
        {
            area = SQ1Q2Os - TQ1Q2Os - S2;
        }
        else // // the centre of the sun is outside the ellipse
        {
            double area_shadow =  SQ1Q2Os - TQ1Q2Os + S2;
            area = 3.14159265357*Rs*Rs -area_shadow;
        }
        
        if(area < 0.0 || area > 3.1415926*Rs*Rs)
        {
            int testc = 0;
        }
        
        return area;

    }
    
    double GMotionState::area_ellispe(double Q1[2], double Q2[2], double Os[2], double Rs,double a, double b, bool in_out)
    {
        double area = 0.0;
        
        double as[2] = {Q1[0]-Os[0],Q1[1] -Os[1] };
        double bs[2] = {Q2[0]-Os[0],Q2[1] -Os[1] };
        
        double ae[2] = {Q1[0],Q1[1]  };
        double be[2] = {Q2[0],Q2[1]  };
        
        
        double TQ1Q2Os = 0.5*fabs(as[0]*bs[1] - bs[0]*as[1]);
        double TQ1Q2Oe = 0.5*fabs(ae[0]*be[1] - be[0]*ae[1]);
        
        double cts = (as[0]*bs[0] + as[1]*bs[1] )/sqrt( (as[0]*as[0]+as[1]*as[1]) * (bs[0]*bs[0]+bs[1]*bs[1]) );
        double SQ1Q2Os = 0.5 * acos(cts)*Rs*Rs;
        
        //
        //double cte = (ae[0]*be[0] + ae[1]*be[1] )/sqrt( (ae[0]*ae[0]+ae[1]*ae[1]) * (be[0]*be[0]+be[1]*be[1]) );
        // elliptical sector
        //double SQ1Q2Oe = 0.5*a*b*acos(cte);
        
        double SQ1Q2Oe = 0.0;
        double aa =  a> b? a: b;
        double q1 =0.0, q2 =0.0;
        if(ae[0]>0 && ae[1]>0)  // the first quadrant
        {
            q1 = atan(ae[1]/ ae[0]);
        }
        else if(ae[0]<0 && ae[1]>0)
        {
            q1 = 3.14159265357 - atan(-ae[1]/ae[0]);
        }
        else if(ae[0]<0 && ae[1]<0)
        {
            q1 = 3.14159265357 + atan(ae[1]/ ae[0]);
        }
        else if( ae[0]>0 && ae[1]<0 )
        {
            q1 = 3.14159265357*2 - atan(-ae[1]/ae[0]);
        }
        
        if(be[0]>0 && be[1]>0)  // the first quadrant
        {
            q2 = atan(be[1]/be[0]);
        }
        else if(be[0]<0 && be[1]>0)
        {
            q2 = 3.14159265357 - atan(-be[1]/be[0]);
        }
        else if(be[0]<0 && be[1]<0)
        {
            q2 = 3.14159265357 + atan(be[1]/be[0]);
        }
        else if( be[0]>0 && be[1]<0 )
        {
            q2 = 3.14159265357*2 - atan(-be[1]/be[0]);
        }
        
        double s_a = a*b/2.0*( q1 - atan2( (b-a)*sin(2.0*q1), (a+b) + (b-a)*cos(2*q1)) );
        double s_b = a*b/2.0*( q2 - atan2( (b-a)*sin(2.0*q2), (a+b) + (b-a)*cos(2*q2)) );
        
        SQ1Q2Oe = fabs(s_b - s_a);
        
//        if(ae[1]*be[1]<0.0)
//        {
//            SQ1Q2Oe = 0.5*a*b*fabs( acos(fabs(ae[0])/aa) + acos(fabs(be[0])/aa) );
//        }
//        else
//        {
//            SQ1Q2Oe = 0.5*a*b*fabs( acos(ae[0]/aa) - acos(be[0]/aa) );
//        }
        
        double S1 = SQ1Q2Oe - TQ1Q2Oe;
        
        if(in_out == true) // the centre of the sun is inside the ellipse
        {
            area = SQ1Q2Os - TQ1Q2Os - S1;
        }
        else // // the centre of the sun is outside the ellipse
        {
            double area_shadow =  SQ1Q2Os - TQ1Q2Os + S1;
            area = 3.14159265357*Rs*Rs -area_shadow;
        }
        
        return area;
    }
    
    // -1: totoally blocked
    //  0: not blocked
    //  1: hyperbolar
    //  2: elliptic
    
   int GMotionState::perspectiveProjection(double a, double b,GVector& sunpos_eci, GVector& satpos_eci, double& r_solar,double& S, double* EC_intersection, double* EC)
    {
        
        //the parameters for the earth ellipsoid
        double Rs = 695700.0; //km
        
        //double a = 6378.137; //km
        //double b = 6356.7523142; //km  6356.7523142
        
        //considering atmosphere
        //double a = 6378.137 + 50; //km
        //double b = 6356.7523142*(a/6378.137); //km  6356.7523142
        
        
        double ab = a*b;
        double a2 = a*a;
        double b2 = b*b;
        double ab2 = ab*ab;
        
        double dis_sat_earth = satpos_eci.norm();
        
        
       // double factor = 1.0;
        
        GVector s; // the origin of the photo coordinate system
        // these are the external orientation elements
        GVector u,v,n;
        
        n = satpos_eci - sunpos_eci;
        n.normalise();
        
        double dis_sat_sun = (satpos_eci - sunpos_eci).norm();
        
        
        // satellite and sun colinear
        
        // first test if the satellite is in the front of earth,
        // if it is in the front of earth, it is always full phase
        // otherwise,using the photogrammetry method
        int state = -1;
        
        double ss = n.x*n.x*a2 + n.y*n.y*a2 + n.z*n.z*b2;
        double s1= sqrt(1.0/ss);
        double s2= -sqrt(1.0/ss);
        
        double t1 = (1.0/s1 - dotproduct(satpos_eci, n))/dotproduct(n, n);
        double t2 = (1.0/s2 - dotproduct(satpos_eci, n))/dotproduct(n, n);
        
        GVector xA = satpos_eci + t1*n;
        GVector xB = satpos_eci + t2*n;
        
        double disA = (xA-sunpos_eci).norm();
        double disB = (xB-sunpos_eci).norm();
        double disMin = disA <= disB? disA: disB;
        
        double disMax = disA >= disB? disA: disB;
        
        //calculate the position of E1 and E2
        GVector E1,E2;
        E1.x = s1*a2*n.x;
        E1.y = s1*a2*n.y;
        E1.z = s1*b2*n.z;
        
        E2.x = s2*a2*n.x;
        E2.y = s2*a2*n.y;
        E2.z = s2*b2*n.z;
        
        GVector m = (xA-E1);
        m.normalise();
        
        ss = m.x*m.x*a2+m.y*m.y*a2+m.z*m.z*a2;
        s1 = sqrt(1.0/ss);
        s2 = -sqrt(1.0/ss);
        double d1 = 1.0/s1 - dotproduct(xA, m);
        double d2 = 1.0/s2 - dotproduct(xA, m);
        GVector xx; // the coordinate of the tangent point on the earth
        ss = fabs(d1)<=fabs(d2)?s1:s2;
        xx.x = ss*a2*m.x;
        xx.y = ss*a2*m.y;
        xx.z = ss*b2*m.z;
        
        double param = dotproduct(satpos_eci+xx,m);
        GVector xP = xx + param*m;
        
        double dis1 = (xP - sunpos_eci).norm();
        
        //printf("%.6f %.6f %.6f %.6f ", dis1, dis_sat_sun, disA, disB);
        
        if( dis_sat_sun <= dis1 ) // full phase
        {
            state = 0;
            return state;
        }
        else if( dis_sat_sun >= dis1 && dis_sat_sun <= disMax ) // what's in the photo is hyperbola
        {
            state = 1;  // hyperbola
        }
        else if(dis_sat_sun >= disMax) // what's in the photo is ellipse
        {
            state = 2;
        }
        
        
        
        GVector r = satpos_eci ;//+ (3.5*dis_sat_earth)*n; // the coordinate of view point, it will affect the penumbra
        
        // this is the so called internal orientation elements, x0=y0=0.0 in this case
        //*****************************************************************
        double f = 1000.0; // the focal distance 1m = 0.001 km, changing f will change the solution situation which is wired !!!!
        
        s = r + f*n; // the origin of the photo coordinate system
        
        /*
         the definiton of photo coordinate system:
         the origin is projection of center of sun
         the most situation:
         u is from origin to the projection of center of earth
         n is from sun to satellite
         v form a right-hand system
         */
        
        // determine the photo-x axis vector
        //calculat the projection of eci_Z on the fundamental plane
        //u = eci_Z - dotproduct(eci_Z, n)/n.norm()/n.norm()*n;
        //u = n.norm()*n.norm()/dotproduct(eci_Z, n)*eci_Z - n;
        //u.normalise();
        
        GVector d = r;
        d.normalise();
        
        double t = 0.0;
        t = f/dotproduct(d, n);
        GVector Xorigin = r + t*d; // the coordinate of the projection of earth center
        
        u = Xorigin - s;
        u.normalise();
        
        v = crossproduct(n, u);
        v.normalise();
        
        
        // the coordinates of earth centre on the projection
        GVector earth_centre;
        earth_centre.x = dotproduct(u, Xorigin-s);
        earth_centre.y = dotproduct(v, Xorigin-s);
        earth_centre.z = dotproduct(n, Xorigin-s);
        
        
        
        GMatrix M(3,3);//A(3,3);
        //A(0,0) = 1.0/a2; A(1,1)=1.0/a2; A(2,2)= 1.0/b2;
        
        t = (r.x*r.x/a2 + r.y*r.y/a2 + r.z*r.z/b2 - 1.0);
        
        M(0,0) = r.x*r.x/a2/a2 - t/a2;
        M(0,1) = r.x*r.y/a2/a2;
        M(0,2) = r.x*r.z/a2/b2;
        M(1,0) = M(0,1);
        M(1,1) = r.y*r.y/a2/a2 - t/a2;
        M(1,2) = r.y*r.z/a2/b2;
        M(2,0) = M(0,2);
        M(2,1) = M(1,2);
        M(2,2) = r.z*r.z/b2/b2 - t/b2;
        
        M = M*4.0;
        
        //cout<<M<<endl;
        
        //calculate K
        double K[6] = {0.0};
        GMatrix tu(3,1), tv(3,1), ts(3,1),tr(3,1),tn(3,1);
        tu[0] = u.x;tu[1] = u.y;tu[2] = u.z;
        tv[0] = v.x;tv[1] = v.y;tv[2] = v.z;
        ts[0] = s.x;ts[1] = s.y;ts[2] = s.z;
        tr[0] = r.x;tr[1] = r.y;tr[2] = r.z;
        tn[0] = n.x;tn[1] = n.y;tn[2] = n.z;
        
        ((~tu)*M*tu).getData(K); // K[0]
        ((~tu)*M*tv).getData(K+1); // K[1]
        ((~tv)*M*tv).getData(K+2); // K[2]
        (2.0*f*(~tu*M*tn)).getData(K+3); //K[3]
        (2.0*f*(~tv*M*tn)).getData(K+4);  //K[4]
        (f*f*(~tn*M*tn)).getData(K+5); //K[5]
        
        
        for(int i =0; i< 6; i++ )
        {
            K[i] = K[i]*1.0E6;
        }
        
        if( K[0]<0.0 && K[2] <0.0 )  // B is negative definite,  make sure D is positive definite
        {
            for( int i=0; i< 6; i++ )
            {
                K[i] = -K[i];
            }
        }
        
        double scale = (r - sunpos_eci).norm(); // the distance from sun to viewpoint
        // the radius of sun on the photo, it is a circle
        double rs = f/scale*Rs;
        
        r_solar = rs;
        double S_solar = 3.14159265357*rs*rs;
        
        
        double XX[4]={0.0}; // the solution of quartic equation
        int num_of_solution = 0;
        
        //calculate the eigen value and eigen vector of matrix B
        double tt =  (K[0]+K[2])*(K[0]+K[2]) - 4.0*(K[0]*K[2] - K[1]*K[1]) ;
        int test2 = tt>0?1:-1;
        tt = sqrt(tt);
        double lambda1 = (K[0]+K[2]+tt)/2.0;
        double lambda2 = (K[0]+K[2]-tt)/2.0;
        
        //get the eigen vector
        double  r1[2]={0.0,1.0},r2[2]={1.0,0.0};
        bool sol1 = false, sol2 =false;
        if( fabs(lambda1 - K[0])<1.0E-12)
        {
            r1[1] = 0.0;
            if( (lambda1 - K[0])*K[1] <0.0 ) // 异号
            {
                r1[0] = -1.0;
            }
            else
            {
                r1[0] = 1.0;
            }
            sol1 = true;
        }
        
        if( fabs(lambda2 - K[2])<1.0E-12)
        {
            r2[0] = 0.0;
            if( (lambda2 - K[2])*K[1] <0.0 ) // 异号
            {
                r2[1] = -1.0;
            }
            else
            {
                r2[1] = 1.0;
            }
            sol2 = true;
        }
        
        if(sol1 ==false)
        {
            r1[0] = K[1]/(lambda1 - K[0]);  // r1 for lambda1
        }
        
        if(sol2 ==false)
        {
            r2[1] = K[1]/(lambda2 - K[2]);  // r2 for lambda2
        }
        
        
        
        //get the unit vector
        double tmp = sqrt(r1[0]*r1[0]+r1[1]*r1[1]);
        r1[0] = r1[0]/tmp;r1[1] = r1[1]/tmp;
        
        tmp = sqrt(r2[0]*r2[0]+r2[1]*r2[1]);
        r2[0] = r2[0]/tmp;r2[1] = r2[1]/tmp;
        
        //the larger eigen value is lambda1
        if( lambda1 <= lambda2 )  // swap lamda1 and lamda2, with r1 and r2
        {
            double t = lambda1;
            lambda1 = lambda2;
            lambda2 = t;
            double r[2] ={r1[0],r1[1]};
            r1[0] = r2[0]; r1[1] = r2[1];
            r2[0] = r[0]; r2[1] = r[1];
        }
        
        //apply the rotation transformation
        double Y[2]= {0.0};
        Y[0] = r1[0]*K[3] + r2[0]*K[4];
        Y[1] = r1[1]*K[3] + r2[1]*K[4];
        
        //double EC[2]={0.0};
        EC[0] = r1[0]*earth_centre.x + r2[0]*earth_centre.y;
        EC[1] = r1[1]*earth_centre.x + r2[1]*earth_centre.y;
        
        
        //calculate phi
        double phi = (  Y[0]*Y[0]/4.0/lambda1 + Y[1]*Y[1]/4.0/lambda2 - K[5] )/lambda1/lambda2;
        
        double CC[2] ={ -Y[0]/2.0/lambda1, -Y[1]/2.0/lambda2 }; // center X and Y
        
        //calculate the semi-major axis and semi-minor axis
        // since the curve can be a hyperbolor, thus
        double AA = lambda2*phi; // x axis, a^2 , including sign
        double BB = lambda1*phi; // y axis, b^2, including sign
        
        
        // ellipsoid
        if(state == 2)
        {
            double n[2]={ EC[0]/sqrt(EC[0]*EC[0] + EC[1]*EC[1] ) , EC[1]/sqrt(EC[0]*EC[0] + EC[1]*EC[1] )  };
            
            double ma = BB*n[0]*n[0] + AA*n[1]*n[1];
            double mb = -2.0*( n[0]*CC[0]*BB + n[1]*CC[1]*AA  );
            double mc = BB*CC[0]*CC[0] + AA*CC[1]*CC[1] - AA*BB;
            
            double t1 = (-mb + sqrt(mb*mb - 4.0*ma*mc))/(2.0*ma);
            double t2 = (-mb - sqrt(mb*mb - 4.0*ma*mc))/(2.0*ma);
            double test = CC[0]*CC[0]/AA + CC[1]*CC[1]/BB - 1.0;
            if(test <=0.0)
            {
                if(t1 < 0)
                {
                    t = t1;
                }
                if(t2 < 0)
                {
                    t = t2;
                }
            }
            else
            {
                if(t1 > 0)
                {
                    t = t1;
                }
                if(t2 > 0)
                {
                    t = t2;
                }
            }
            
            EC_intersection[0] = t*n[0];
            EC_intersection[1] = t*n[1];
            
            
        }
        else if(state == 1)  // hyperbola
        {
            
            double n[2]={ EC[0]/sqrt(EC[0]*EC[0] + EC[1]*EC[1] ) , EC[1]/sqrt(EC[0]*EC[0] + EC[1]*EC[1] )  };
            
            double ma = BB*n[0]*n[0] + AA*n[1]*n[1];
            double mb = -2.0*( n[0]*CC[0]*BB + n[1]*CC[1]*AA  );
            double mc = BB*CC[0]*CC[0] + AA*CC[1]*CC[1] - AA*BB;
            double t1 = (-mb + sqrt(mb*mb - 4.0*ma*mc))/(2.0*ma);
            double t2 = (-mb - sqrt(mb*mb - 4.0*ma*mc))/(2.0*ma);
            
            double test = CC[0]*CC[0]/AA + CC[1]*CC[1]/BB - 1.0;
            if(test >=0.0)
            {
                if(t1 < 0)
                {
                    t = t1;
                }
                if(t2 < 0)
                {
                    t = t2;
                }
            }
            else
            {
                if(t1 > 0)
                {
                    t = t1;
                }
                if(t2 > 0)
                {
                    t = t2;
                }
            }
            
            EC_intersection[0] = t*n[0];
            EC_intersection[1] = t*n[1];
            int testc = 0;
        }
        
        //dis_earth_boundary =  fabs( sqrt( (EC[0]-CC[0])*(EC[0]-CC[0]) + (EC[1]-CC[1])*(EC[1]-CC[1])   ) - sqrt(AA));
        //dis_earth_sun_proj   = earth_centre.norm();
        
        
        //then solve the quartic equation
        // ref: http://users.nik.uni-obuda.hu/sanyo/gpgpu/sami2015_submission_60.pdf
        // using Ferrari method
        
        //first, forming the intersection quartic equation
        t = BB*(rs+CC[0])*(rs+CC[0]) + AA*CC[1]*CC[1] - AA*BB; // A
        double A = 1.0;
        double B = (-4.0*AA*rs*CC[1])/t;
        double C = (2.0*AA*CC[1]*CC[1] + 2.0*BB*CC[0]*CC[0] + 4.0*AA*rs*rs - 2.0*BB*rs*rs  - 2.0*AA*BB)/t;
        double D = (-4.0*AA*rs*CC[1])/t;
        double E = (BB*(rs-CC[0])*(rs-CC[0]) + AA*CC[1]*CC[1] - AA*BB)/t;
        
        // A=-15;B=0;C=376;D=0;E=-28; // for the set of x^2+y^2=9 and x^2/16+y^2/4=1
        //A = -1; B=0;C=0;D=0;E=1;
        // just need to find out all the real solutions
        
        double ce[4]={B,C,D,E};
        
        num_of_solution = GMath::quarticSolver(ce,XX);
        
        if( num_of_solution == 3 || num_of_solution == 4)
        {
            printf("WARNING: shadowfactor, %d intersections!\n", num_of_solution);
            exit(0);
        }
        
        //cout<<"num_of_solution:"<< num_of_solution<<endl;
        //printf("%2d %8.6f %8.6f   %8.6f %8.6f   %12.8f %12.8f %12.8f %12.8f %12.8f %12.8f   %12.8f %12.8f %12.8f\n",
        //       num_of_solution,AA,BB,CC[0],CC[1],K[0],K[1],K[2],K[3],K[4],K[5], lambda1,lambda2,phi);
        
        //then testing, whether the center of sun is inside the earth, this is for penumbra
        
        
        if( AA*BB <=0.0 )  // hyperbola
        {
            double test =  CC[0]*CC[0]/AA + CC[1]*CC[1]/BB;
            if(test <= 1.0 && num_of_solution <2)
            {
                state = 0;
                return state;
            }
            
            if(test > 1.0 && num_of_solution <2)
            {
                state = -1; //umbra
                return state;
            }
            
            // get the coordinates of two intersections
            double xc1[2]={0,0},xc2[2]={0.0}; // the coordinate of C and C'
            xc1[0] = (1.0-XX[0]*XX[0])/(1.0+XX[0]*XX[0])*rs;
            xc1[1] = (2.0*XX[0])/(1.0+XX[0]*XX[0])*rs;
            
            xc2[0] = (1.0-XX[1]*XX[1])/(1.0+XX[1]*XX[1])*rs;
            xc2[1] = (2.0*XX[1])/(1.0+XX[1]*XX[1])*rs;
            
            // the area of triangle POP'
            double TOPP = 0.5*fabs( xc1[0]*xc2[1] - xc2[0]*xc1[1]);
            
            // the area of circular sector OPP', O is the centre of the sun, while P and P' are the two intersections
            double t1 = sqrt(xc1[0]*xc1[0] + xc1[1]*xc1[1] );
            double t2 = sqrt(xc2[0]*xc2[0] + xc2[1]*xc2[1] );
            double SOPP = 0.5*acos((xc1[0]*xc2[0] + xc1[1]*xc2[1])/t1/t2)*rs*rs;
            
            double S_h = 0.0;
            
            // the area of the hyperbolic sector
            //ref:   http://www.robertobigoni.eu/Matematica/Conics/segmentHyp/segmentHyp.html
            //step1: move the two intersections and the equation of hyperbola to the center of the coordinate system
            double XC1[2] = {xc1[0]- CC[0], xc1[1]- CC[1] }; // first intersection
            double XC2[2] = {xc2[0]- CC[0], xc2[1]- CC[1] }; // second intersection
            
            double a = AA, b =-BB;
            
            
            
            
            if( BB > 0)  // 主轴是y, 整个双曲线旋转90度，使得x为主轴
            {
                // x' = y
                // y' =-x
                double tmp = 0.0;
                tmp = XC1[0]; XC1[0] = XC1[1]; XC1[1] = -tmp;
                tmp = XC2[0]; XC2[0] = XC2[1]; XC2[1] = -tmp;
                
                a = BB; b =-AA;
            }
            
            a = sqrt(a); b= sqrt(b);
            
            
            
            
            if(XC1[0]<=a)
            {
                XC1[0] = a;
            }
            if(XC2[0]<=a)
            {
                XC2[0] = a;
            }
            double s1 = b*( XC1[0]*sqrt(  (XC1[0]*XC1[0]/a/a) -1.0 ) - a*acosh(XC1[0]/a) );
            
            double s2 = b*( XC2[0]*sqrt(  (XC2[0]*XC2[0]/a/a) -1.0 ) - a*acosh(XC2[0]/a) );
            
            double ss = s1<=s2?s1:s2;
            double sl = s1>=s2?s1:s2;
            
            if( XC1[1]*XC2[1] < 0.0 )  //2个交点在不同侧
            {
                S_h = ss + (sl - ss)/2.0;
            }
            else  //2 个交点在同一侧
            {
                double s_trapizium = fabs((XC1[0] - XC2[0]) )*( (fabs(XC1[1]) + fabs(XC2[1]) ) );
                S_h = (sl - ss - s_trapizium )/2.0;
            }
            
            if( test <= 1.0 )  // center of sun is inside hyperbola, but outside of earth. different strategy for calculation or area ratio
            {
                state = 1;
                S = S_solar- (SOPP - TOPP + S_h);
            }
            else if( test >= 1.0)
            {
                state = 1;
                S = SOPP - TOPP - S_h;
            }
        }
        else if( AA*BB >=0.0 )  // ellipse
        {
            double test =  CC[0]*CC[0]/AA + CC[1]*CC[1]/BB;
            
            if( test < 1.0  )  // center of sun is inside earth, different strategy for calculation or area ratio
            {
                if( num_of_solution < 2 )  //umbra
                {
                    state = -1;
                }
                else //penumbra
                {
                    //calculating the area ratio
                    state = 2;
                    //it seems it is impossible to get solution more than 2
                    double xc1[2]={0,0},xc2[2]={0.0}; // the coordinate of C and C'
                    xc1[0] = (1.0-XX[0]*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    xc1[1] = (2.0*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    
                    xc2[0] = (1.0-XX[1]*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    xc2[1] = (2.0*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    
                    // the area of triangle TACC' , 2d cross product
                    double TACC = 0.5*fabs( xc1[0]*xc2[1] - xc2[0]*xc1[1]);
                    double TBCC = 0.5*fabs( (xc1[0]-CC[0])*(xc2[1]-CC[1]) - (xc2[0]-CC[0])*(xc1[1]-CC[1]) );
                    
                    // the area of the circular part
                    double t1 = sqrt(xc1[0]*xc1[0] + xc1[1]*xc1[1] );
                    double t2 = sqrt(xc2[0]*xc2[0] + xc2[1]*xc2[1] );
                    double SectorACC = 0.5*acos((xc1[0]*xc2[0] + xc1[1]*xc2[1])/t1/t2)*rs*rs;
                    
                    double SectorBCC = 0.0;
                    double d[2]={0.0},e[2]={0.0};
                    //calculating the area of elliptic sector
                    if( AA >= BB )  // semi-major axis is x axis
                    {
                        //以半径为R画个圆
                        double R = sqrt(AA);
                        double y1 = CC[1]+sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        double y2 = CC[1]-sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        d[0] = xc1[0];
                        // get the nearest point
                        d[1] = fabs(y1 - xc1[1]) <= fabs(y2 - xc1[1]) ? y1 : y2;
                        
                        y1 = CC[1]+sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        y2 = CC[1]-sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        
                        e[0] = xc2[0];
                        // get the nearest point
                        e[1] = fabs(y1 - xc2[1]) <= fabs(y2 - xc2[1]) ? y1 : y2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        if(Acbc >1.0) Acbc =1.0;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                    }
                    else if(AA <= BB) // semi-major axis is y axis
                    {
                        double R = sqrt(BB);
                        
                        double x1 = CC[0]+sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        double x2 = CC[0]-sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        d[1] = xc1[1];
                        // get the nearest point
                        d[0] = fabs( x1 - xc1[0]) <= fabs(x2 - xc1[0]) ? x1 : x2;
                        
                        x1 = CC[0]+sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        x2 = CC[0]-sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        e[1] = xc2[1];
                        // get the nearest point
                        e[0] = fabs(x1 - xc2[0]) <= fabs(x2 - xc2[0]) ? x1 : x2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        Acbc = acos(Acbc);
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                    }
                    
                    
                    // the visible sun disk
                    S = (SectorACC - TACC) - (SectorBCC - TBCC);
                    
                    //factor =  fabs(S/(3.14159265357*rs*rs));
                    
                    //factor = 0.5;
                    //int testc = 0;
                    
                }
            }
            else // the center of sun is outside of the earth
            {
                if( num_of_solution < 2 )  //full phase
                {
                    state = 0;
                }
                else  // penumbra
                {
                    //calculate the area ratio
                    state = 2;
                    //it seems it is impossible to get solution more than 2
                    double xc1[2]={0,0},xc2[2]={0.0}; // the coordinate of C and C'
                    xc1[0] = (1.0-XX[0]*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    xc1[1] = (2.0*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    
                    xc2[0] = (1.0-XX[1]*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    xc2[1] = (2.0*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    
                    
                    // the area of triangle TACC' , 2d cross product
                    
                    double TACC = 0.5*fabs( xc1[0]*xc2[1] - xc2[0]*xc1[1]);
                    double TBCC = 0.5*fabs( (xc1[0]-CC[0])*(xc2[1]-CC[1]) - (xc2[0]-CC[0])*(xc1[1]-CC[1]) );
                    
                    // the area of the circular part
                    double t1 = sqrt(xc1[0]*xc1[0] + xc1[1]*xc1[1] );
                    double t2 = sqrt(xc2[0]*xc2[0] + xc2[1]*xc2[1] );
                    double SectorACC = 0.5*acos((xc1[0]*xc2[0] + xc1[1]*xc2[1])/rs/rs)*rs*rs;
                    
                    double SectorBCC = 0.0;
                    double d[2]={0.0},e[2]={0.0};
                    
                    //calculating the area of elliptic sector
                    if( AA >= BB )  // semi-major axis is x axis
                    {
                        double R = sqrt(AA);
                        
                        double y1 = CC[1]+sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        double y2 = CC[1]-sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        d[0] = xc1[0];
                        // get the nearest point
                        d[1] = fabs(y1 - xc1[1]) <= fabs(y2 - xc1[1]) ? y1 : y2;
                        
                        y1 = CC[1]+sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        y2 = CC[1]-sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        e[0] = xc2[0];
                        // get the nearest point
                        e[1] = fabs(y1 - xc2[1]) <= fabs(y2 - xc2[1]) ? y1 : y2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        Acbc = acos(Acbc);
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                        
                    }
                    else if(AA <= BB) // semi-major axis is y axis
                    {
                        double R = sqrt(BB);
                        
                        double x1 = CC[0]+sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        double x2 = CC[0]-sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        d[1] = xc1[1];
                        // get the nearest point
                        d[0] = fabs(x1 - xc1[0]) <= fabs(x2 - xc1[0]) ? x1 : x2;
                        
                        x1 = CC[0]+sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        x2 = CC[0]-sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        e[1] = xc2[1];
                        // get the nearest point
                        e[0] = fabs(x1 - xc2[0]) <= fabs(x2 - xc2[0]) ? x1 : x2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        Acbc = acos(Acbc);
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                    }
                    
                    double S0 = (SectorACC - TACC) + (SectorBCC - TBCC);
                    
                    S = S_solar - S0;
                    
                    //factor = 1.0 - fabs(S/(3.14159265357*rs*rs));
                    
                    //factor = 0.5;
                    
                    //int testc = 0;
                    
                }
                
            }
            
        }
        
        return state;
        
    }
    
    double GMotionState::myshadowFactor(GVector& sunpos_eci, GVector& satpos_eci)
    {
        double factor = 1.0;
        
        double a = 6378.137; //km
        double b = 6356.7523142; //km  6356.7523142
        
        // for sphere test
//        double a = 6371.0; //km
//        double b = 6371.0; //km  6356.7523142
//
        double hgt_atm = 50.0; // the hight of atmosphere, this parameter is important
        
        //considering atmosphere
        double a_atm = a + hgt_atm; //km
        double b_atm = b*(a_atm/a); //km  6356.7523142
        
        double Area_earth =0.0, Area_atm = 0.0, Area_solar=0.0;
        int state1 = -1, state2 =-1;
        
//        double EC_intersection1[2]={0.0},EC_intersection2[2]={0.0}, EC[2]={0.0};
//        double Boundary_earth[2] = {0.0}, Boundary_atm[2] = {0.0};
//        double PEC_new[2] ={0.0};
//        double sun_intersection[2]={0.0};
        
        double x1 =0.0, x2 =0.0, dis0_e =0.0,dis0_t =0.0, dis1 = 0.0, dis2 = 0.0, thickness=0.0;
        double r_sun = 0.0;
        
        if(hgt_atm == 0.0 )
        {
            //state2 = perspectiveProjection(a,b,sunpos_eci,satpos_eci,r_sun, Area_earth,EC_intersection2, EC);
             state2 = myperspectiveProjection(a,b,sunpos_eci,satpos_eci,r_sun, Area_earth,dis1, dis0_e);
             Area_solar = 3.14159265357*r_sun*r_sun;
            
            if(state2 == 0)
            {
                factor = 1.0;
            }
            else if (state2 == -1)
            {
                factor =  0.0;
            }
            else
            {
               factor = Area_earth/Area_solar;
            }
        }
        else  // considering the atmosphere
        {
            //double testec[2]={0.0};
            //state1 = perspectiveProjection(a_atm,b_atm,sunpos_eci,satpos_eci,r_sun, Area_atm,EC_intersection1, EC);
            //state2 = perspectiveProjection(a,b,sunpos_eci,satpos_eci,r_sun, Area_earth,EC_intersection2, EC);
            
            state1 = myperspectiveProjection(a_atm,b_atm,sunpos_eci,satpos_eci,r_sun, Area_atm,dis1,dis0_t);
            state2 = myperspectiveProjection(a,b,sunpos_eci,satpos_eci,r_sun, Area_earth,dis2,dis0_e);
            
            double mu1 = 0.0;  // 大气层辐射通过系数, distance = 0
            double mu2 = 1.0;  // distance = 1;
            
            //double mu1 = 0.3;  // 大气层辐射通过系数, distance = 0
            //double mu2 = 1.0;  // distance = 1;
            
            double a_linear = mu1;
            double b_linear = mu2;
            
            // so the linear model would be y = 0.6x + 0.3
            double a_exp = mu1;
            double b_exp = std::log(mu2/mu1);
            
            double a_log = mu1;
            double b_log = (mu2-mu1)/std::log(2.0);
            
            Area_solar = 3.14159265357*r_sun*r_sun;
            
            // the distance from earth centre to the intersection of atmosphere
//           dis1 = sqrt( ( PEC_new[0] - Boundary_atm[0]  )*(PEC_new[0] - Boundary_atm[0] )
//                               + (PEC_new[1] - Boundary_atm[1] )*(PEC_new[1] - Boundary_atm[1] )  );
//
//           dis2 = sqrt( ( PEC_new[0] - Boundary_earth[0]  )*(PEC_new[0] - Boundary_earth[0])
//                        + (PEC_new[1] - Boundary_earth[1] )*(PEC_new[1] - Boundary_earth[1]) );
//
//            // the distance from earth centre to the intersection of solar circle
//           dis0 = sqrt( ( PEC_new[0] - sun_intersection[0]  )*(PEC_new[0] - sun_intersection[0] )
//                               + (PEC_new[1] - sun_intersection[1] )*(PEC_new[1] - sun_intersection[1] )  );
            
            // the thickness of atmosphere
           thickness = dis1 - dis2;
           
            //printf("%6.4f %6.4f %6.4f %6.4f\n",dis0,dis1,dis2,thickness);
            //线性模型
            //double u1 = (mu2-mu1) * x1 + mu1;
            //double u2 = (mu2-mu1) * x2 + mu1;
            
            //指数模型
            //double u1 = a_exp*exp(b_exp*x1);
            //double u2 = a_exp*exp(b_exp*x2);
            
            //对数模型
            //double u1 = a_log + b_log*log(x1+1.0);
            //double u2 = a_log + b_log*log(x2+1.0);
            
            //double u = a_log + b_log*log(x+1.0);
            
            // S 曲线模型 Logistic function  y = L/(1 + aexp(bx));
            //double u1 = 1.0/( 1+ exp(-3*x1+1) );
            //double u2 = 1.0/( 1+ exp(-3*x2+1) );
            
            if(state1 == 0)  // outside of atmosphere
            {
                factor = 1.0;
            }
            // partly in the atmosphere
            if((state1 ==1 || state1 == 2 )
               && state2 == 0
               )
            {
                x1 = 1.0;
                x2 = (thickness - (dis1 - dis0_t))/thickness;
                
                //log
                //double u1 = a_log + b_log*log(x1+1.0);
                //double u2 = a_log + b_log*log(x2+1.0);
                
                //linear
                double u1 = (mu2-mu1) * x1 + mu1;
                double u2 = (mu2-mu1) * x2 + mu1;
                
                double coeff = (u1+u2)/2.0;
                //double coeff = u;
                factor = (Area_atm + (Area_solar - Area_atm)*coeff )/Area_solar;
                //factor = 0.9;
            }
            
            // totally in the atmosphere
            if( state1 == -1 && state2 == 0 )
            {
                double u1 =0, u2 =0;
                if(dis2 != 0.0)
                {
                    x1 = (dis0_t - dis2)/thickness;
                    x2 = (dis0_t - dis2 + 2.0*r_sun )/thickness;
                    
                    //log
                    //double u1 = a_log + b_log*log(x1+1.0);
                    //double u2 = a_log + b_log*log(x2+1.0);
                    
                    //linear
                    u1 = (mu2-mu1) * x1 + mu1;
                    u2 = (mu2-mu1) * x2 + mu1;
                }
                else if (fabs(dis2)< 1.0E-9) // no projection for the solid Earth (outside of the solid Earth), but the satellite is in the atmosphere's projection
                {
                    
                    //thickness = hgt_atm*dis1 / ((a_atm+b_atm)/2.0 );
//                    thickness = 500*r_sun;
//                    x1 = (dis1 - dis0  - 2*r_sun)/thickness;
//                    x2 = (dis1 - dis0)/thickness;
//
//                    //linear
//                    u1 = (mu1-mu2) * x1 + mu2;
//                    u2 = (mu1-mu2) * x2 + mu2;
//
                    u1 = 0.5;
                    u2 = 0.5;
                    
                }
                
                
                
                factor = (u1+u2)/2.0;
                //factor = u;
            }
            // partly in the earth and atmosphere
            if( state1 == -1 && (state2 == 1  || state2 == 2) )
            {
                x1 = (2.0*r_sun - (dis2 - dis0_t))/thickness;
                x2 = 0.0;
                
                //log
                //double u1 = a_log + b_log*log(x1+1.0);
                //double u2 = a_log + b_log*log(x2+1.0);
                
                //linear
                double u1 = (mu2-mu1) * x1 + mu1;
                double u2 = (mu2-mu1) * x2 + mu1;
                
                double coeff = (u1 + u2)/2.0;
                
                factor = (Area_earth*coeff )/Area_solar;
            }
            
            // totally in the earth's shadow, umbra
            // in umbra, the CERES data will capture the atmosphere effect
            if( state2 == -1  )
            {
                factor = 0.0;
                //printf("cccc\n");
            }
            
            // the sun is very big, bigger than the atmosphere thickness
            if( (state1 ==1 || state1 == 2)
               && (state2 ==1 || state2 == 2)
              )
            {
                //需要分成3段, 关键确定第2段大气层中的辐射系数
                double area1 = Area_atm;
                double area2 = Area_earth - Area_atm;
                //double area3 = Area_solar - area1 - area2;
                
                x1 =  1.0;
                x2 =  0.0;
                //对数模型
                //double u1 = a_log + b_log*log(x1+1.0);
                //double u2 = a_log + b_log*log(x2+1.0);
                
                //linear
                double u1 = (mu2-mu1) * x1 + mu1;
                double u2 = (mu2-mu1) * x2 + mu1;
                
                factor = (area1*1.0 + area2*(u1+u2)/2.0)/Area_solar;
            }
        }
        
        //printf("%f %f %f \n", factor, f1, f2);
        //printf("%f %d %d %f %f %f %f %f %f %f %f\n", factor, state1, state2, Area_solar, Area_atm, Area_earth,mydis, mydis1, mydis2, x1, x2 );
        
        //printf("%f %d %d %f %f %f %f %f %f\n", factor, state1, state2, dis0,dis1, dis2, thickness,Area_atm,Area_earth);
        
        return factor;
    }
    
    // the shadow factor function to decide the scale of flux in eclipse
    /*
     * the basic idea here is put a camera on the satellite and take a photo of earth and sun,
     * and then all the calculation is done on this photo
     overlap area of ellipses
     ref:  https://arxiv.org/pdf/1106.3787.pdf
     
     // the area of right hyperbolic sector
     http://keisan.casio.com/exec/system/14770153128432
     
     */
    double GMotionState::shadowFactor(double a, double b, GVector& sunpos_eci, GVector& satpos_eci)
    {
        
        //the parameters for the earth ellipsoid
        double Rs = 695700.0; //km
        
        //double a = 6378.137; //km
        //double b = 6356.7523142; //km  6356.7523142
        
        //considering atmosphere
        //double a = 6378.137 + 50; //km
        //double b = 6356.7523142*(a/6378.137); //km  6356.7523142
        
        
        //double ab = a*b;
        double a2 = a*a;
        double b2 = b*b;
        
        //double ab2 = ab*ab;
        
        //double dis_sat_earth = satpos_eci.norm();
        
        double factor = 1.0;
        
        GVector s; // the origin of the photo coordinate system
        // these are the external orientation elements
        GVector u,v,n;
        
        n = satpos_eci - sunpos_eci;
        n.normalise();
        
        double dis_sat_sun = (satpos_eci - sunpos_eci).norm();
        
        
        // satellite and sun colinear
        
        
        
        // first test if the satellite is in the front of earth,
        // if it is in the front of earth, it is always full phase
        // otherwise,using the photogrammetry method
        int state = -1;
        
        double ss = n.x*n.x*a2 + n.y*n.y*a2 + n.z*n.z*b2;
        double s1= sqrt(1.0/ss);
        double s2= -sqrt(1.0/ss);
        
        double t1 = (1.0/s1 - dotproduct(satpos_eci, n))/dotproduct(n, n);
        double t2 = (1.0/s2 - dotproduct(satpos_eci, n))/dotproduct(n, n);
        
        GVector xA = satpos_eci + t1*n;
        GVector xB = satpos_eci + t2*n;
        
        double disA = (xA-sunpos_eci).norm();
        double disB = (xB-sunpos_eci).norm();
        double disMin = disA <= disB? disA: disB;
        
        double disMax = disA >= disB? disA: disB;
        
        
        //calculate the position of E1 and E2
        GVector E1,E2;
        E1.x = s1*a2*n.x;
        E1.y = s1*a2*n.y;
        E1.z = s1*b2*n.z;
        
        E2.x = s2*a2*n.x;
        E2.y = s2*a2*n.y;
        E2.z = s2*b2*n.z;
        
        GVector m = (xA-E1);
        m.normalise();
        
        ss = m.x*m.x*a2+m.y*m.y*a2+m.z*m.z*a2;
        s1 = sqrt(1.0/ss);
        s2 = -sqrt(1.0/ss);
        double d1 = 1.0/s1 - dotproduct(xA, m);
        double d2 = 1.0/s2 - dotproduct(xA, m);
        GVector xx; // the coordinate of the tangent point on the earth
        ss = fabs(d1)<=fabs(d2)?s1:s2;
        xx.x = ss*a2*m.x;
        xx.y = ss*a2*m.y;
        xx.z = ss*b2*m.z;
        
        double param = dotproduct(satpos_eci+xx,m);
        GVector xP = xx + param*m;
        
        double dis1 = (xP - sunpos_eci).norm();
        
        //printf("%.6f %.6f %.6f %.6f ", dis1, dis_sat_sun, disA, disB);
        
        if( dis_sat_sun <= dis1 ) // full phase
        {
            state = 0;
            return 1.0;
        }
        else if( dis_sat_sun >= dis1 && dis_sat_sun <= disMax ) // what's in the photo is hyperbola
        {
            state = 1;  // hyperbola
        }
        else if(dis_sat_sun >= disMax) // what's in the photo is ellipse
        {
            state = 2;
        }
        
        
        
        GVector r = satpos_eci ;//+ (3.5*dis_sat_earth)*n; // the coordinate of view point, it will affect the penumbra
        
        // this is the so called internal orientation elements, x0=y0=0.0 in this case
        //*****************************************************************
        double f = 1000.0; // the focal distance 1m = 0.001 km, changing f will change the solution situation which is wired !!!!
        
        s = r + f*n; // the origin of the photo coordinate system
        
        /*
         the definiton of photo coordinate system:
         the origin is projection of center of sun
         the most situation:
         u is from origin to the projection of center of earth
         n is from sun to satellite
         v form a right-hand system
         */
        
        // determine the photo-x axis vector
        //calculat the projection of eci_Z on the fundamental plane
        //u = eci_Z - dotproduct(eci_Z, n)/n.norm()/n.norm()*n;
       //u = n.norm()*n.norm()/dotproduct(eci_Z, n)*eci_Z - n;
       //u.normalise();
        
        GVector d = r;
        d.normalise();
       
        double t = 0.0;
        t = f/dotproduct(d, n);
        GVector Xorigin = r + t*d; // the coordinate of the projection of earth center
        
        u = Xorigin - s;
        u.normalise();
        
        v = crossproduct(n, u);
        v.normalise();
        
        GMatrix M(3,3);//A(3,3);
        //A(0,0) = 1.0/a2; A(1,1)=1.0/a2; A(2,2)= 1.0/b2;
        
        t = (r.x*r.x/a/a + r.y*r.y/a/a + r.z*r.z/b/b - 1.0);
        
        M(0,0) = r.x*r.x/a2/a2 - t/a2;
        M(0,1) = r.x*r.y/a2/a2;
        M(0,2) = r.x*r.z/a2/b2;
        M(1,0) = M(0,1);
        M(1,1) = r.y*r.y/a2/a2 - t/a2;
        M(1,2) = r.y*r.z/a2/b2;
        M(2,0) = M(0,2);
        M(2,1) = M(1,2);
        M(2,2) = r.z*r.z/b2/b2 - t/b2;
        
        M = M*4.0;
        
        //cout<<M<<endl;
        
        //calculate K
        double K[6] = {0.0};
        GMatrix tu(3,1), tv(3,1), ts(3,1),tr(3,1),tn(3,1);
        tu[0] = u.x;tu[1] = u.y;tu[2] = u.z;
        tv[0] = v.x;tv[1] = v.y;tv[2] = v.z;
        ts[0] = s.x;ts[1] = s.y;ts[2] = s.z;
        tr[0] = r.x;tr[1] = r.y;tr[2] = r.z;
        tn[0] = n.x;tn[1] = n.y;tn[2] = n.z;
        
        ((~tu)*M*tu).getData(K); // K[0]
        ((~tu)*M*tv).getData(K+1); // K[1]
        ((~tv)*M*tv).getData(K+2); // K[2]
        (2.0*f*(~tu*M*tn)).getData(K+3); //K[3]
        (2.0*f*(~tv*M*tn)).getData(K+4);  //K[4]
        (f*f*(~tn*M*tn)).getData(K+5); //K[5]
        
        
        for(int i =0; i< 6; i++ )
        {
            K[i] = K[i]*1.0E6;
        }
        
        if( K[0]<0.0 && K[2] <0.0 )  // B is negative definite,  make sure D is positive definite
        {
            for( int i=0; i< 6; i++ )
            {
                K[i] = -K[i];
            }
        }
        
        double scale = (r - sunpos_eci).norm(); // the distance from sun to viewpoint
        // the radius of sun on the photo, it is a circle
        double rs = f/scale*Rs;
        
        
        double XX[4]={0.0}; // the solution of quartic equation
        int num_of_solution = 0;
        
        double Kcoef[5] = {0.0};
        Kcoef[0] = K[0]*rs*rs - K[3]*rs + K[5];
        Kcoef[1] = 2*K[4]*rs - 4*K[1]*rs*rs;
        Kcoef[2] = 4.0*K[2]*rs*rs - 2.0*K[0]*rs*rs + 2.0*K[5];
        Kcoef[3] = 4.0*K[1]*rs*rs + 2.0*K[4]*rs;
        Kcoef[4] = K[0]*rs*rs + K[3]*rs + K[5];
        double Kce[4] = {Kcoef[1]/Kcoef[0],Kcoef[2]/Kcoef[0],Kcoef[3]/Kcoef[0],Kcoef[4]/Kcoef[0] };
        
        num_of_solution = GMath::quarticSolver(Kce,XX);
        if( num_of_solution == 3 || num_of_solution == 4)
        {
            printf("WARNING: shadowfactor, %d intersections!", num_of_solution);
            int testc = 0;
        }
        
        //calculate the eigen value and eigen vector of matrix B
        double tt =  (K[0]+K[2])*(K[0]+K[2]) - 4.0*(K[0]*K[2] - K[1]*K[1]) ;
        int test2 = tt>0?1:-1;
        tt = sqrt(tt);
        double lambda1 = (K[0]+K[2]+tt)/2.0;
        double lambda2 = (K[0]+K[2]-tt)/2.0;
        
        //get the eigen vector
        double  r1[2]={0.0,1.0},r2[2]={1.0,0.0};
        bool sol1 = false, sol2 =false;
        if( fabs(lambda1 - K[0])<1.0E-12)
        {
            r1[1] = 0.0;
            if( (lambda1 - K[0])*K[1] <0.0 ) // 异号
            {
                r1[0] = -1.0;
            }
            else
            {
                r1[0] = 1.0;
            }
            sol1 = true;
        }
        
        if( fabs(lambda2 - K[2])<1.0E-12)
        {
            r2[0] = 0.0;
            if( (lambda2 - K[2])*K[1] <0.0 ) // 异号
            {
                r2[1] = -1.0;
            }
            else
            {
                r2[1] = 1.0;
            }
            sol2 = true;
        }
        
        if(sol1 ==false)
        {
            r1[0] = K[1]/(lambda1 - K[0]);  // r1 for lambda1
        }
        
        if(sol2 ==false)
        {
            r2[1] = K[1]/(lambda2 - K[2]);  // r2 for lambda2
        }
       
        
        
        //get the unit vector
        double tmp = sqrt(r1[0]*r1[0]+r1[1]*r1[1]);
        r1[0] = r1[0]/tmp;r1[1] = r1[1]/tmp;
        
        tmp = sqrt(r2[0]*r2[0]+r2[1]*r2[1]);
        r2[0] = r2[0]/tmp;r2[1] = r2[1]/tmp;
        
        //the larger eigen value is lambda1
        if( lambda1 <= lambda2 )  // swap lamda1 and lamda2, with r1 and r2
        {
            double t = lambda1;
            lambda1 = lambda2;
            lambda2 = t;
            double r[2] ={r1[0],r1[1]};
            r1[0] = r2[0]; r1[1] = r2[1];
            r2[0] = r[0]; r2[1] = r[1];
        }
        
        double Y[2]= {0.0};
        Y[0] = r1[0]*K[3] + r2[0]*K[4];
        Y[1] = r1[1]*K[3] + r2[1]*K[4];
        
        
        
        //calculate phi
        double phi1 = Y[0]*Y[0]/4.0/lambda1 + Y[1]*Y[1]/4.0/lambda2 - K[5];
        
        double phi = (  Y[0]*Y[0]/4.0/lambda1 + Y[1]*Y[1]/4.0/lambda2 - K[5] )/lambda1/lambda2;
        
        double CC[2] ={ -Y[0]/2.0/lambda1, -Y[1]/2.0/lambda2 }; // center X and Y
        
        //calculate the semi-major axis and semi-minor axis
        // since the curve can be a hyperbolor, thus
        double AA = lambda2*phi; // x axis, a^2 , including sign
        double BB = lambda1*phi; // y axis, b^2, including sign
        
        
        //then solve the quartic equation
        // ref: http://users.nik.uni-obuda.hu/sanyo/gpgpu/sami2015_submission_60.pdf
        // using Ferrari method
        
        //first, forming the intersection quartic equation
        t = BB*(rs+CC[0])*(rs+CC[0]) + AA*CC[1]*CC[1] - AA*BB; // A
        double A = 1.0;
        double B = (-4.0*AA*rs*CC[1])/t;
        double C = (2.0*AA*CC[1]*CC[1] + 2.0*BB*CC[0]*CC[0] + 4.0*AA*rs*rs - 2.0*BB*rs*rs  - 2.0*AA*BB)/t;
        double D = (-4.0*AA*rs*CC[1])/t;
        double E = (BB*(rs-CC[0])*(rs-CC[0]) + AA*CC[1]*CC[1] - AA*BB)/t;
        
        // A=-15;B=0;C=376;D=0;E=-28; // for the set of x^2+y^2=9 and x^2/16+y^2/4=1
        //A = -1; B=0;C=0;D=0;E=1;
        // just need to find out all the real solutions
        
        double ce[4]={B,C,D,E};
        
        num_of_solution = GMath::quarticSolver(ce,XX);
        
        if( num_of_solution == 3 || num_of_solution == 4)
        {
            printf("WARNING: shadowfactor, %d intersections!\n", num_of_solution);
            exit(0);
        }

        //cout<<"num_of_solution:"<< num_of_solution<<endl;
        //printf("%2d %8.6f %8.6f   %8.6f %8.6f   %12.8f %12.8f %12.8f %12.8f %12.8f %12.8f   %12.8f %12.8f %12.8f\n",
        //       num_of_solution,AA,BB,CC[0],CC[1],K[0],K[1],K[2],K[3],K[4],K[5], lambda1,lambda2,phi);
        
        //then testing, whether the center of sun is inside the earth, this is for penumbra
        
        
        if( AA*BB <=0.0 )  // hyperbola
        {
             double test =  CC[0]*CC[0]/AA + CC[1]*CC[1]/BB;
             if( test <= 1.0 )
             {
                 if( num_of_solution <2 )
                 {
                     factor = 1.0;   //full phase
                 }
                 else  // penumbra
                 {
                     //printf("Hyperbola on the photo_1!\n");
                     factor = 0.5;
                     int testc =0;
                 }
             }
            else if( test >= 1.0)
            {
                if( num_of_solution <2 )
                {
                    factor = 0.0;  // umbra
                }
                else  // penumbra
                {
                    //printf("Hyperbola on the photo_2!\n");
                    factor = 0.5;
                    int testc = 0;
                }
            }
        }
        else if( AA*BB >=0.0 )  // ellipse
        {
            double test =  CC[0]*CC[0]/AA + CC[1]*CC[1]/BB;
            
            if( test < 1.0  )  // center of sun is inside earth, different strategy for calculation or area ratio
            {
                if( num_of_solution < 2 )  //umbra
                {
                    factor = 0.0;
                }
                else //penumbra
                {
                    //calculating the area ratio
                    
                    //it seems it is impossible to get solution more than 2
                    double xc1[2]={0,0},xc2[2]={0.0}; // the coordinate of C and C'
                    xc1[0] = (1.0-XX[0]*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    xc1[1] = (2.0*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    
                    xc2[0] = (1.0-XX[1]*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    xc2[1] = (2.0*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    
                    // the area of triangle TACC' , 2d cross product
                    double TACC = 0.5*fabs( xc1[0]*xc2[1] - xc2[0]*xc1[1]);
                    double TBCC = 0.5*fabs( (xc1[0]-CC[0])*(xc2[1]-CC[1]) - (xc2[0]-CC[0])*(xc1[1]-CC[1]) );
                    
                    // the area of the circular part
                    double t1 = sqrt(xc1[0]*xc1[0] + xc1[1]*xc1[1] );
                    double t2 = sqrt(xc2[0]*xc2[0] + xc2[1]*xc2[1] );
                    double SectorACC = 0.5*acos((xc1[0]*xc2[0] + xc1[1]*xc2[1])/t1/t2)*rs*rs;
                    
                    double SectorBCC = 0.0;
                    double d[2]={0.0},e[2]={0.0};
                    //calculating the area of elliptic sector
                    if( AA >= BB )  // semi-major axis is x axis
                    {
                        //以半径为R画个圆
                        double R = sqrt(AA);
                        double y1 = CC[1]+sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        double y2 = CC[1]-sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        d[0] = xc1[0];
                        // get the nearest point
                        d[1] = fabs(y1 - xc1[1]) <= fabs(y2 - xc1[1]) ? y1 : y2;
                        
                        y1 = CC[1]+sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        y2 = CC[1]-sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        
                        e[0] = xc2[0];
                        // get the nearest point
                        e[1] = fabs(y1 - xc2[1]) <= fabs(y2 - xc2[1]) ? y1 : y2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        if(Acbc >1.0) Acbc =1.0;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                    }
                    else if(AA <= BB) // semi-major axis is y axis
                    {
                        double R = sqrt(BB);
                        
                        double x1 = CC[0]+sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        double x2 = CC[0]-sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        d[1] = xc1[1];
                        // get the nearest point
                        d[0] = fabs( x1 - xc1[0]) <= fabs(x2 - xc1[0]) ? x1 : x2;
                        
                        x1 = CC[0]+sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        x2 = CC[0]-sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        e[1] = xc2[1];
                        // get the nearest point
                        e[0] = fabs(x1 - xc2[0]) <= fabs(x2 - xc2[0]) ? x1 : x2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        Acbc = acos(Acbc);
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                    }
                    
                    
                    
                    double S = (SectorACC - TACC) - (SectorBCC - TBCC);
                    
                    factor =  fabs(S/(3.14159265357*rs*rs));
                    
                    //factor = 0.5;
                    int testc = 0;
                    
                }
            }
            else // the center of sun is outside of the earth
            {
                if( num_of_solution < 2 )  //full phase
                {
                    
                    factor = 1.0;
                    
                }
                else  // penumbra
                {
                    //calculate the area ratio
                    
                    //it seems it is impossible to get solution more than 2
                    double xc1[2]={0,0},xc2[2]={0.0}; // the coordinate of C and C'
                    xc1[0] = (1.0-XX[0]*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    xc1[1] = (2.0*XX[0])/(1.0+XX[0]*XX[0])*rs;
                    
                    xc2[0] = (1.0-XX[1]*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    xc2[1] = (2.0*XX[1])/(1.0+XX[1]*XX[1])*rs;
                    
                    
                    // the area of triangle TACC' , 2d cross product
                    
                    double TACC = 0.5*fabs( xc1[0]*xc2[1] - xc2[0]*xc1[1]);
                    double TBCC = 0.5*fabs( (xc1[0]-CC[0])*(xc2[1]-CC[1]) - (xc2[0]-CC[0])*(xc1[1]-CC[1]) );
                    
                    // the area of the circular part
                    double t1 = sqrt(xc1[0]*xc1[0] + xc1[1]*xc1[1] );
                    double t2 = sqrt(xc2[0]*xc2[0] + xc2[1]*xc2[1] );
                    double SectorACC = 0.5*acos((xc1[0]*xc2[0] + xc1[1]*xc2[1])/rs/rs)*rs*rs;
                    
                    double SectorBCC = 0.0;
                    double d[2]={0.0},e[2]={0.0};
                    
                    //calculating the area of elliptic sector
                    if( AA >= BB )  // semi-major axis is x axis
                    {
                        double R = sqrt(AA);
                        
                        double y1 = CC[1]+sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        double y2 = CC[1]-sqrt(R*R - (xc1[0]-CC[0])*(xc1[0]-CC[0]));
                        d[0] = xc1[0];
                        // get the nearest point
                        d[1] = fabs(y1 - xc1[1]) <= fabs(y2 - xc1[1]) ? y1 : y2;
                        
                        y1 = CC[1]+sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        y2 = CC[1]-sqrt(R*R - (xc2[0]-CC[0])*(xc2[0]-CC[0]));
                        e[0] = xc2[0];
                        // get the nearest point
                        e[1] = fabs(y1 - xc2[1]) <= fabs(y2 - xc2[1]) ? y1 : y2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        Acbc = acos(Acbc);
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                        
                    }
                    else if(AA <= BB) // semi-major axis is y axis
                    {
                        double R = sqrt(BB);
                        
                        double x1 = CC[0]+sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        double x2 = CC[0]-sqrt( R*R - (xc1[1]-CC[1])*(xc1[1]-CC[1]));
                        d[1] = xc1[1];
                        // get the nearest point
                        d[0] = fabs(x1 - xc1[0]) <= fabs(x2 - xc1[0]) ? x1 : x2;
                        
                        x1 = CC[0]+sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        x2 = CC[0]-sqrt( R*R - (xc2[1]-CC[1])*(xc2[1]-CC[1]));
                        e[1] = xc2[1];
                        // get the nearest point
                        e[0] = fabs(x1 - xc2[0]) <= fabs(x2 - xc2[0]) ? x1 : x2;
                        
                        //double xc1[2] = {x1[0],0.0},xc2[2]={x2[0],0.0};
                        //angle CBC', in fact, it should be Angle DBE
                        double tmp1[2] = {d[0]-CC[0],d[1]-CC[1]};
                        double tmp2[2] = {e[0]-CC[0],e[1]-CC[1]};
                        t1 = sqrt(tmp1[0]*tmp1[0]+tmp1[1]*tmp1[1]);
                        t2 = sqrt(tmp2[0]*tmp2[0]+tmp2[1]*tmp2[1]);
                        double Acbc = (tmp1[0]*tmp2[0]+tmp1[1]*tmp2[1])/t1/t2;
                        
                        if(Acbc >1.0) Acbc =1.0;
                        else if(Acbc<-1.0) Acbc = -1.0;
                        
                        Acbc = acos(Acbc);
                        SectorBCC = 0.5*Acbc*sqrt(AA*BB);
                        
                    }
                    
                    double S = (SectorACC - TACC) + (SectorBCC - TBCC);
                    
                    factor = 1.0 - fabs(S/(3.14159265357*rs*rs));
                    
                    //factor = 0.5;
                    
                    int testc = 0;
                    
                }
                
            }
            
        }
        
        return factor;
        
    }
    
    
    
    
    
} // end of namespace
