// @HEADER
// *****************************************************************************
//                           Intrepid2 Package
//
// Copyright 2007 NTESS and the Intrepid2 contributors.
// SPDX-License-Identifier: BSD-3-Clause
// *****************************************************************************
// @HEADER


/** \file
    \brief  Test of the CellTools class.
    \author Kyungjoo Kim
*/

#include "Intrepid2_config.h"
#include "Intrepid2_CellTopologyTags.hpp"

#include "Intrepid2_DefaultCubatureFactory.hpp"

#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_ScalarTraits.hpp"
#include "Intrepid2_CellTools.hpp"

namespace Intrepid2 {
  
  namespace Test {
#define INTREPID2_TEST_ERROR_EXPECTED( S )                              \
    try {                                                               \
      S ;                                                               \
    }                                                                   \
    catch (std::logic_error &err) {                                      \
      *outStream << "Expected Error ----------------------------------------------------------------\n"; \
      *outStream << err.what() << '\n';                                 \
      *outStream << "-------------------------------------------------------------------------------" << "\n\n"; \
    };                                                                  


#define INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, expected, shtopo, celltag) \
    {                                                                   \
      typedef shtopo shardsTopology;                                    \
      typedef celltag CellTopologyTag;                                  \
                                                                        \
      const auto order = 3;                                             \
      const auto cellTopo = shards::CellTopology(shards::getCellTopologyData< shardsTopology >()); \
      auto cub = DefaultCubatureFactory::create<DeviceType,ValueType,ValueType>(cellTopo, order); \
      const auto P = cub->getNumPoints();                               \
      const auto D = 3;                                                 \
                                                                        \
      Kokkos::DynRankView<ValueType,DeviceType> pts("pts", P, D);  \
      Kokkos::DynRankView<ValueType,DeviceType> wts("wts", P);     \
      Kokkos::DynRankView<int,DeviceType> check("check", P);       \
                                                                        \
      cub->getCubature(pts, wts);                                       \
                                                                        \
      Kokkos::RangePolicy<ExecSpaceType> policy(0, P);                \
      typedef F_checkPointInclusion<CellTopologyTag,decltype(check),decltype(pts)> FunctorType; \
      Kokkos::parallel_for(policy, FunctorType(offset, check, pts));    \
                                                                        \
      auto check_host = Kokkos::create_mirror_view(check);              \
      Kokkos::deep_copy(check_host, check);                             \
                                                                        \
      for (ordinal_type i=0;i<P;++i) {                                  \
        const double diff = std::abs(check_host(i) - expected);         \
        if (diff > tol) {                                               \
          *outStream << "Error : checkPointInclusion at ("              \
                     << i                                               \
                     << ") with diff = " << diff << "\n";               \
          errorFlag++;                                                  \
        }                                                               \
      }                                                                 \
    }                                                                   \
      
    template<typename cellTopologyTagType,
             typename OutputViewType,
             typename inputViewType>
    struct F_checkPointInclusion {
      double _offset;
      OutputViewType _output;
      inputViewType _input;

      KOKKOS_INLINE_FUNCTION
      F_checkPointInclusion(const double offset_, 
                            OutputViewType output_,
                            inputViewType input_)
        : _offset(offset_), 
          _output(output_), 
          _input(input_) {}

      KOKKOS_INLINE_FUNCTION
      void
      operator()(const ordinal_type i) const {
        const auto in = Kokkos::subview(_input,i,Kokkos::ALL());
        for (int k=0;k<3;++k) in(k)+=_offset;
        const auto check = cellTopologyTagType::checkPointInclusion(in, 0.0);
        
        _output(i) = check;        
      }
    };


    // We provide maps belonging to the functional spaces generated by the finite element basis functions associated to the each topology  

    template<size_t CellTopologyKey>
    struct MapPoints;

    template <>
    struct MapPoints<shards::Line<2>::key> {
      double operator()(const double* coords, int /*comp*/) {const auto& x = coords[0]; return 2.0+3*x;}
    };

    template <>
    struct MapPoints<shards::Line<3>::key> {
      double operator()(const double* coords, int /*comp*/) {const auto& x = coords[0]; return 2.0+3*x-0.1*x*x;}
    };

    template <>
    struct MapPoints<shards::Triangle<3>::key> {
      double operator()(const double* coords, int comp) {
        const auto& x = coords[0]; const auto& y = coords[1];
        return (comp == 0) ? 2.0+3*x+2*y
                           : -2.0+2*x+5*y;
      }
    };

    template <>
    struct MapPoints<shards::Triangle<6>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Triangle<3>::key> linearMap;
        const auto& x = coords[0]; const auto& y = coords[1];
        return linearMap(coords, comp) -0.1*x*x+0.05*x*y+0.2*y*y;
      }
    };

    template <>
    struct MapPoints<shards::Quadrilateral<4>::key> {
      double operator()(const double* coords, int comp) {
        const auto& x = coords[0]; const auto& y = coords[1];
        return (comp == 0) ? 2.0+3*x+2*y+0.1*x*y
                           : -2.0+2*x+5*y-0.1*x*y;
      }
    };

    template <>
    struct MapPoints<shards::Quadrilateral<8>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Quadrilateral<4>::key> bilinearMap;
        const auto& x = coords[0]; const auto& y = coords[1];
        return bilinearMap(coords, comp)-0.1*x*x -0.2*y*y +0.05*x*y*(x-y);
      }
    };

    template <>
    struct MapPoints<shards::Quadrilateral<9>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Quadrilateral<4>::key> bilinearMap;
        const auto& x = coords[0]; const auto& y = coords[1];
        return bilinearMap(coords, comp)-0.1*x*x -0.2*y*y +0.05*x*y*(x-y+x*y);
      }
    };

    template <>
    struct MapPoints<shards::Tetrahedron<4>::key> {
      double operator()(const double* coords, int comp) {
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return (comp == 0) ? 2.0+3*x+2*y+4*z :
               (comp == 1) ? -2.0+2*x+5*y+4*z :
                             -3.0 +2*x +1*y +3*z;
      }
    };

    template <>
    struct MapPoints<shards::Tetrahedron<10>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Tetrahedron<4>::key> linearMap;
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return linearMap(coords, comp)-0.1*x*x -0.2*y*y +0.3*z*z +0.05*x*y +0.07*x*z -0.06*y*z;  //for simplicity we choose the same higher-order terms for all components 
      }
    };


    template <>
    struct MapPoints<shards::Pyramid<5>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Tetrahedron<4>::key> linearMap;
        const double eps = epsilon();
        const auto& x = coords[0]; const auto& y = coords[1]; const auto z = (1.0 - coords[2] < eps) ? 1.0-eps : coords[2];
        return linearMap(coords, comp)-0.1*x*y/(1.0-z);  //for simplicity we choose the same higher-order terms for all components 
      }
    };

    template <>
    struct MapPoints<shards::Pyramid<13>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Tetrahedron<10>::key> quadraticMap;
        const double eps = epsilon();
        const auto& x = coords[0]; const auto& y = coords[1]; const auto z = (1.0 - coords[2] < eps) ? 1.0-eps : coords[2];
        return quadraticMap(coords, comp)-0.1*x*y/(1.0-z)*(1.0-x+y);  //for simplicity we choose the higher-order terms for all components 
      }
    };
    
    template <>
    struct MapPoints<shards::Wedge<6>::key> {
      double operator()(const double* coords, int comp) {
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return (comp == 0) ? 2.0+3*x+2*y+4*z  +0.07*x*z -0.06*y*z :
               (comp == 1) ? -2.0+2*x+5*y+4*z +0.07*x*z -0.06*y*z :
                             -3.0+2*x+1*y+3*z +0.07*x*z -0.06*y*z;
      }
    };

    template <>
    struct MapPoints<shards::Wedge<15>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Wedge<6>::key> wedge6Map;
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return wedge6Map(coords, comp)-0.05*(x*x - y*y + z*z +  x*y + x*y*z + y*z*z + x*x*z + y*y*z + x*z*z);  //for simplicity we choose the same higher-order terms for all components 
      }
    };

    template <>
    struct MapPoints<shards::Wedge<18>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Wedge<15>::key> wedge15Map;
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return wedge15Map(coords, comp)+0.04*(x*x*z*z -  x*y*z*z + y*y*z*z);  //for simplicity we choose the same higher-order terms for all components 
      }
    };

    template <>
    struct MapPoints<shards::Hexahedron<8>::key> {
      double operator()(const double* coords, int comp) {
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return (comp == 0) ? 2.0+3*x+2*y+4*z  +0.05*x*y +0.07*x*z -0.06*y*z + 0.05*x*y*z :
               (comp == 1) ? -2.0+2*x+5*y+4*z +0.05*x*y +0.07*x*z -0.06*y*z + 0.05*x*y*z :
                             -3.0+2*x+1*y+3*z +0.05*x*y +0.07*x*z -0.06*y*z + 0.05*x*y*z;
      }
    };

    template <>
    struct MapPoints<shards::Hexahedron<20>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Hexahedron<8>::key> trilinearMap;
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return trilinearMap(coords, comp)-0.05*(x*x - y*y + z*z +  x*y*z*(x-y-z)+ x*x*y + y*z*z + x*x*z + y*y*z+ x*y*y + x*z*z);  //for simplicity we choose the same higher-order terms for all components 
      }
    };

    template <>
    struct MapPoints<shards::Hexahedron<27>::key> {
      double operator()(const double* coords, int comp) {
        MapPoints<shards::Hexahedron<20>::key> hex20Map;
        const auto& x = coords[0]; const auto& y = coords[1]; const auto& z = coords[2];
        return hex20Map(coords, comp)+0.07*(x*x*y*y + x*x*z*z + y*y*z*z + x*x*y*y*z*z   + x*y*z*(x*y-x*z+y*z));  //for simplicity we choose the same higher-order terms for all components 
      }
    };



    double mapLine2(const double* coords, int /*comp*/) {const auto& x = coords[0]; return 2.0+3*x;}

// This macro computes the reference cell nodes and maps them to the physical space (according to functional space associated to the topology).
// It also maps four input points to the same physical sapce.
#define INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, host_points, physPoints, physNodes, shtopo, cellTools)     \
    {                                                                                                                            \
        MapPoints<shtopo::key> mapPoints;                                                                                        \
        Intrepid2::HostBasisPtr<double, double> basisPtr;                                                                        \
        using HostDeviceType = Kokkos::HostSpace::device_type;                                                                   \
        shards::CellTopology topo( shards::getCellTopologyData<shtopo>());                                                       \
        switch (topo.getKey()) {                                                                                                 \
          case shards::Line<2>::key:          basisPtr = Teuchos::rcp(new Basis_HGRAD_LINE_C1_FEM   <HostDeviceType>()); break;  \
          case shards::Line<3>::key:          basisPtr = Teuchos::rcp(new Basis_HGRAD_LINE_C2_FEM   <HostDeviceType>()); break;  \
          case shards::Triangle<3>::key:      basisPtr = Teuchos::rcp(new Basis_HGRAD_TRI_C1_FEM    <HostDeviceType>()); break;  \
          case shards::Quadrilateral<4>::key: basisPtr = Teuchos::rcp(new Basis_HGRAD_QUAD_C1_FEM   <HostDeviceType>()); break;  \
          case shards::Tetrahedron<4>::key:   basisPtr = Teuchos::rcp(new Basis_HGRAD_TET_C1_FEM    <HostDeviceType>()); break;  \
          case shards::Hexahedron<8>::key:    basisPtr = Teuchos::rcp(new Basis_HGRAD_HEX_C1_FEM    <HostDeviceType>()); break;  \
          case shards::Wedge<6>::key:         basisPtr = Teuchos::rcp(new Basis_HGRAD_WEDGE_C1_FEM  <HostDeviceType>()); break;  \
          case shards::Pyramid<5>::key:       basisPtr = Teuchos::rcp(new Basis_HGRAD_PYR_C1_FEM    <HostDeviceType>()); break;  \
          case shards::Triangle<6>::key:      basisPtr = Teuchos::rcp(new Basis_HGRAD_TRI_C2_FEM    <HostDeviceType>()); break;  \
          case shards::Quadrilateral<8>::key: basisPtr = Teuchos::rcp(new Basis_HGRAD_QUAD_I2_FEM   <HostDeviceType>()); break;  \
          case shards::Quadrilateral<9>::key: basisPtr = Teuchos::rcp(new Basis_HGRAD_QUAD_C2_FEM   <HostDeviceType>()); break;  \
          case shards::Tetrahedron<10>::key:  basisPtr = Teuchos::rcp(new Basis_HGRAD_TET_C2_FEM    <HostDeviceType>()); break;  \
          case shards::Tetrahedron<11>::key:  basisPtr = Teuchos::rcp(new Basis_HGRAD_TET_COMP12_FEM<HostDeviceType>()); break;  \
          case shards::Hexahedron<20>::key:   basisPtr = Teuchos::rcp(new Basis_HGRAD_HEX_I2_FEM    <HostDeviceType>()); break;  \
          case shards::Hexahedron<27>::key:   basisPtr = Teuchos::rcp(new Basis_HGRAD_HEX_C2_FEM    <HostDeviceType>()); break;  \
          case shards::Wedge<15>::key:        basisPtr = Teuchos::rcp(new Basis_HGRAD_WEDGE_I2_FEM  <HostDeviceType>()); break;  \
          case shards::Wedge<18>::key:        basisPtr = Teuchos::rcp(new Basis_HGRAD_WEDGE_C2_FEM  <HostDeviceType>()); break;  \
          case shards::Pyramid<13>::key:      basisPtr = Teuchos::rcp(new Basis_HGRAD_PYR_I2_FEM    <HostDeviceType>()); break;  \
          default:   {};                                                                                                         \
        }                                                                                                                        \
                                                                                                                                 \
        auto dim = topo.getDimension();                                                                                          \
        auto refNodes_h = Kokkos::DynRankView<ValueType,Kokkos::HostSpace>("refNodes",basisPtr->getCardinality(),dim);           \
        basisPtr->getDofCoords(refNodes_h);                                                                                      \
        physNodes = Kokkos::DynRankView<ValueType,DeviceType>("physNodes",1,basisPtr->getCardinality(),dim);                     \
        physPoints = Kokkos::DynRankView<ValueType,DeviceType>("physPoints", host_points.extent(0),dim);                        \
        auto physNodes_h = Kokkos::create_mirror_view(physNodes);                                                                \
        auto physPoints_h = Kokkos::create_mirror_view(physPoints);                                                              \
        double coords[3]={};                                                                                                     \
        for(size_t i=0; i< refNodes_h.extent(0);++i) {                                                                           \
          for (size_t d=0; d<dim; ++d)                                                                                           \
            coords[d]= refNodes_h(i,d);                                                                                          \
          for (size_t d=0; d<dim; ++d)                                                                                           \
            physNodes_h(0,i,d) = mapPoints(coords,d);                                                                            \
        }                                                                                                                        \
        for(size_t i=0; i< host_points.extent(0);++i) {                                                                          \
          for (size_t d=0; d<dim; ++d)                                                                                           \
            coords[d]= host_points(i,d);                                                                                       \
          for (size_t d=0; d<dim; ++d)                                                                                           \
            physPoints_h(i,d) = mapPoints(coords,d);                                                                           \
        }                                                                                                                        \
        Kokkos::deep_copy(physPoints,physPoints_h);                                                                              \
        Kokkos::deep_copy(physNodes,physNodes_h);                                                                                \
    }                                                                                                                            \
        
// This macro performs the inclusion test for four points and check that they are inside/outside the cell as expected.
// We expect that the first and third points are inside the cell, whereas the second and fourth are outside.
#define INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, points, physNodes, shtopo, cellTools)     \
    {                                                                                   \
        shards::CellTopology topo( shards::getCellTopologyData<shtopo>());              \
        assert(ct::hasReferenceCell(topo));                                             \
        std::string label = "reference";                                                \
        if(physNodes.extent(0)==0)                                                      \
          cellTools::checkPointwiseInclusion(Kokkos::subview(inCell,0,Kokkos::ALL()),   \
            points, topo );                                                             \
        else {                                                                          \
          cellTools::checkPointwiseInclusion(inCell, points, physNodes, topo );         \
          label = "physical";                                                           \
        }                                                                               \
        auto inCell_host = Kokkos::create_mirror_view(inCell);                          \
        Kokkos::deep_copy(inCell_host, inCell);                                         \
        auto numPoints = points.extent(0);                                              \
        if (inCell_host(0,0)==0) {                                                      \
          *outStream << "Error : Point 0 is inside the " <<label <<" element " <<       \
          topo.getName() << " but PointWiseInclusion says otherwise";                   \
          errorFlag++;                                                                  \
        }                                                                               \
        if (inCell_host(0,1)==1)     {                                                  \
          *outStream << "Error : Point 1 is outside the " <<label <<" element " <<      \
          topo.getName() << " but PointWiseInclusion says otherwise";                   \
          errorFlag++;                                                                  \
        }                                                                               \
        if (numPoints>2 && inCell_host(0,2)==0)     {                                   \
          *outStream << "Error : Point 2 is inside the " <<label <<" element " <<       \
          topo.getName() << " but PointWiseInclusion says otherwise";                   \
          errorFlag++;                                                                  \
        }                                                                               \
        if (numPoints>3 && inCell_host(0,3)==1)     {                                   \
          *outStream << "Error : Point 3 is outside the " <<label <<" element " <<      \
          topo.getName() << " but PointWiseInclusion says otherwise";                   \
          errorFlag++;                                                                  \
        }                                                                               \
      }                                                                                 \
    
        
    template<typename ValueType, typename DeviceType>
    int CellTools_Test07(const bool verbose) {

      using ExecSpaceType = typename DeviceType::execution_space;

      Teuchos::RCP<std::ostream> outStream;
      Teuchos::oblackholestream bhs; // outputs nothing

      if (verbose)
        outStream = Teuchos::rcp(&std::cout, false);
      else
        outStream = Teuchos::rcp(&bhs,       false);

      Teuchos::oblackholestream oldFormatState;
      oldFormatState.copyfmt(std::cout);

      *outStream
        << "===============================================================================\n"
        << "|                                                                             |\n"
        << "|                              Unit Test CellTools                            |\n"
        << "|                                                                             |\n"
        << "|     1) check point inclusion and cell topology tag tests                    |\n"
        << "|                                                                             |\n"
        << "===============================================================================\n";
  
      const ValueType tol = tolerence()*100.0;

      int errorFlag = 0;
      
      try {
        
        *outStream
          << "\n"
          << "===============================================================================\n" 
          << "| Test 1: test cubature points\n"
          << "===============================================================================\n\n";

        {
          double offset = 0.0;
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Line<>,          Impl::Line<2>);
          
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Triangle<>,      Impl::Triangle<3>);
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Quadrilateral<>, Impl::Quadrilateral<4>);
          
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Tetrahedron<>,   Impl::Tetrahedron<4>);
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Hexahedron<>,    Impl::Hexahedron<8>);
          
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Pyramid<>,       Impl::Pyramid<5>);
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, true, shards::Wedge<>,         Impl::Wedge<6>);
        }
        {
          double offset = 3.0;
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Line<>,          Impl::Line<2>);
          
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Triangle<>,      Impl::Triangle<3>);
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Quadrilateral<>, Impl::Quadrilateral<4>);
          
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Tetrahedron<>,   Impl::Tetrahedron<4>);
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Hexahedron<>,    Impl::Hexahedron<8>);
          
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Pyramid<>,       Impl::Pyramid<5>);
          INTREPID2_TEST_CHECK_POINT_INCLUSION(offset, false, shards::Wedge<>,         Impl::Wedge<6>);
        }



      } catch (std::logic_error &err) {
        //============================================================================================//
        // Wrap up test: check if the test broke down unexpectedly due to an exception                //
        //============================================================================================//
        *outStream << err.what() << "\n";
        errorFlag = -1000;
      }

            try {
        
        *outStream
          << "\n"
          << "===============================================================================\n" 
          << "| Test 2: test Point wise inclusion points\n"
          << "===============================================================================\n\n";


        // Here we check point wise inclusion on a single cell for different topologies
        // We check both the case where the input points are in the reference space and the can where they are in the physical space
        // In the physical-space case, we map the points according to the geometric mapping space defined by the topology        
        // For each topology we consider 4 points. The first two are near a topology vertex, the first is inside the topology the second outside
        // The third and fourth points are near a face/side barycenter. The third one is inside the cell, the fourth outside 

        Kokkos::DynRankView<ValueType,DeviceType> pts1d("pts1d", 4, 1);
        Kokkos::DynRankView<ValueType,DeviceType> pts2d("pts2d", 4, 2);
        Kokkos::DynRankView<ValueType,DeviceType> pts3d("pts3d", 4, 3);
        Kokkos::DynRankView<int,DeviceType> inCell("inCell", 1,4);
        Kokkos::DynRankView<ValueType,DeviceType> emptyView;
        Kokkos::DynRankView<ValueType,DeviceType> physNodes;
        Kokkos::DynRankView<ValueType,DeviceType> physPoints;
        auto pts1d_h = Kokkos::create_mirror_view(pts1d); 
        auto pts2d_h = Kokkos::create_mirror_view(pts2d); 
        auto pts3d_h = Kokkos::create_mirror_view(pts3d); 
        double eps = 1e-4;
        using ct = Intrepid2::CellTools<DeviceType>;

        // line topologies
        pts1d_h(0,0) = -1.0+eps;   //point near vertex (in)
        pts1d_h(1,0) = -1.0-eps;   //point near vertex (out)
        pts1d_h(2,0) = 1.0-eps;    //point near vertex (in)
        pts1d_h(3,0) = 1.0+eps;    //point near vertex (out)
        Kokkos::deep_copy(pts1d,pts1d_h);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts1d, emptyView, shards::Line<2>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts1d, emptyView, shards::Line<3>, ct);

        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts1d_h, physPoints, physNodes, shards::Line<2>, ct);    
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Line<2>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts1d_h, physPoints, physNodes, shards::Line<3>, ct);  
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Line<3>, ct);


        // triangle topologies
        pts2d_h(0,0) = 0.0+eps; pts2d_h(0,1) = 0.0+eps; //point near vertex (in)
        pts2d_h(1,0) = 0.0-eps; pts2d_h(1,1) = 0.0-eps; //point near vertex (out)
        pts2d_h(2,0) = 0.5-eps; pts2d_h(2,1) = 0.5-eps; //point near edge (in)
        pts2d_h(3,0) = 0.5+eps; pts2d_h(3,1) = 0.5+eps; //point near edge (out)
        Kokkos::deep_copy(pts2d,pts2d_h);

        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts2d, emptyView, shards::Triangle<3>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts2d, emptyView, shards::Triangle<4>, ct);

        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts2d_h, physPoints, physNodes, shards::Triangle<3>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Triangle<3>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts2d_h, physPoints, physNodes, shards::Triangle<6>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Triangle<6>, ct);


        // quadrilateral topologies
        pts2d_h(0,0) = -1.0+eps; pts2d_h(0,1) = -1.0+eps;  //point near vertex (in)
        pts2d_h(1,0) = -1.0-eps; pts2d_h(1,1) = -1.0-eps;  //point near vertex (out)
        pts2d_h(2,0) = 0.0; pts2d_h(2,1) = -1.0+eps;       //point near edge (in)
        pts2d_h(3,0) = 0.0; pts2d_h(3,1) = -1.0-eps;       //point near edge (out)
        Kokkos::deep_copy(pts2d,pts2d_h);

        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts2d, emptyView, shards::Quadrilateral<4>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts2d, emptyView, shards::Quadrilateral<8>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts2d, emptyView, shards::Quadrilateral<9>, ct);

        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts2d_h, physPoints, physNodes, shards::Quadrilateral<4>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Quadrilateral<4>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts2d_h, physPoints, physNodes, shards::Quadrilateral<8>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Quadrilateral<8>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts2d_h, physPoints, physNodes, shards::Quadrilateral<9>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Quadrilateral<9>, ct);

        pts3d_h(0,0) = 0.0+eps; pts3d_h(0,1) = 0.0+eps; pts3d_h(0,2) = 0.0+eps;        //point near vertex (in)
        pts3d_h(1,0) = 0.0-eps; pts3d_h(1,1) = 0.0-eps; pts3d_h(1,2) = 0.0-eps;        //point near vertex (out)
        pts3d_h(2,0) = 1./3.-eps; pts3d_h(2,1) = 1./3.-eps; pts3d_h(2,2) = 1./3.-eps;  //point near face (in)
        pts3d_h(3,0) = 1./3.+eps; pts3d_h(3,1) = 1./3.+eps; pts3d_h(3,2) = 1./3.+eps;  //point near face (out)
        Kokkos::deep_copy(pts3d,pts3d_h);


        // tetrahedron topologies
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Tetrahedron<4>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Tetrahedron<10>, ct);

        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Tetrahedron<4>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes  , shards::Tetrahedron<4>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Tetrahedron<10>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Tetrahedron<10>, ct);


        // pyramid topologies
        pts3d_h(0,0) = -1.0+eps; pts3d_h(0,1) = -1.0+eps; pts3d_h(0,2) = 0.0+0.5*eps;  //points near vertex (in)
        pts3d_h(1,0) = -1.0-eps; pts3d_h(1,1) = -1.0-eps; pts3d_h(1,2) = 0.0-0.5*eps;  //points near vertex (out)
        pts3d_h(2,0) = 0.0; pts3d_h(2,1) = 0.0; pts3d_h(2,2) = 0.0+eps;                //points near face (in)
        pts3d_h(3,0) = 0.0; pts3d_h(3,1) = 0.0; pts3d_h(3,2) = 0.0-eps;                //points near face (out)
        Kokkos::deep_copy(pts3d,pts3d_h);

        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Pyramid<5>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Pyramid<13>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Pyramid<14>, ct);
        
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Pyramid<5>, ct);
	      INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Pyramid<5>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Pyramid<13>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Pyramid<13>, ct);
        //missing basis functions for Pyramid<14>, need to implement them


        // wedge topologies
        pts3d_h(0,0) = 0.0+eps; pts3d_h(0,1) = 0.0+eps; pts3d_h(0,2) = -1.0+eps;  //point near vertex (in)
        pts3d_h(1,0) = 0.0-eps; pts3d_h(1,1) = 0.0-eps; pts3d_h(1,2) = -1.0-eps;  //point near vertex (out)
        pts3d_h(2,0) = 0.5-eps; pts3d_h(2,1) = 0.5-eps; pts3d_h(2,2) = 0.0;       //point near face (in)
        pts3d_h(3,0) = 0.5+eps; pts3d_h(3,1) = 0.5+eps; pts3d_h(3,2) = 0.0;       //point near face (out)
        Kokkos::deep_copy(pts3d,pts3d_h);

        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Wedge<6>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Wedge<15>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Wedge<18>, ct);
        
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Wedge<6>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Wedge<6>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Wedge<15>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Wedge<15>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Wedge<18>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Wedge<18>, ct);


        // hexahedron topologies
        pts3d_h(0,0) = -1.0+eps; pts3d_h(0,1) = -1.0+eps; pts3d_h(0,2) = -1.0+eps;  //point near vertex (in)
        pts3d_h(1,0) = -1.0-eps; pts3d_h(1,1) = -1.0-eps; pts3d_h(1,2) = -1.0-eps;  //point near vertex (out)
        pts3d_h(2,0) = 0.0; pts3d_h(2,1) = 0.0; pts3d_h(2,2) = -1.0+eps;            //point near face (in)
        pts3d_h(3,0) = 0.0; pts3d_h(3,1) = 0.0; pts3d_h(3,2) = -1.0-eps;            //point near face (out)
        Kokkos::deep_copy(pts3d,pts3d_h);

        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Hexahedron<8>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Hexahedron<20>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, pts3d, emptyView, shards::Hexahedron<27>, ct);

        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Hexahedron<8>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Hexahedron<8>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Hexahedron<20>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Hexahedron<20>, ct);
        INTREPID2_COMPUTE_POINTS_AND_CELL_NODES_IN_PHYS_SPACE(inCell, pts3d_h, physPoints, physNodes, shards::Hexahedron<27>, ct);
        INTREPID2_TEST_CHECK_POINTWISE_INCLUSION(inCell, physPoints, physNodes, shards::Hexahedron<27>, ct);

      } catch (std::logic_error &err) {
        //============================================================================================//
        // Wrap up test: check if the test broke down unexpectedly due to an exception                //
        //============================================================================================//
        *outStream << err.what() << "\n";
        errorFlag = -1000;
      }
      
      if (errorFlag != 0)
        std::cout << "End Result: TEST FAILED\n";
      else
        std::cout << "End Result: TEST PASSED\n";
      
      // reset format state of std::cout
      std::cout.copyfmt(oldFormatState);
      
      return errorFlag;
    }
  }
}
    

