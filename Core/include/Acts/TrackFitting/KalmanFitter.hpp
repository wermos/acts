// This file is part of the Acts project.
//
// Copyright (C) 2016-2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// Workaround for building on clang+libstdc++
#include "Acts/Utilities/detail/ReferenceWrapperAnyCompat.hpp"

#include "Acts/Definitions/Algebra.hpp"
#include "Acts/EventData/Measurement.hpp"
#include "Acts/EventData/MeasurementHelpers.hpp"
#include "Acts/EventData/MultiTrajectory.hpp"
#include "Acts/EventData/MultiTrajectoryHelpers.hpp"
#include "Acts/EventData/SourceLink.hpp"
#include "Acts/EventData/TrackParameters.hpp"
#include "Acts/EventData/VectorMultiTrajectory.hpp"
#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/MagneticField/MagneticFieldContext.hpp"
#include "Acts/Material/MaterialSlab.hpp"
#include "Acts/Propagator/AbortList.hpp"
#include "Acts/Propagator/ActionList.hpp"
#include "Acts/Propagator/ConstrainedStep.hpp"
#include "Acts/Propagator/DirectNavigator.hpp"
#include "Acts/Propagator/Navigator.hpp"
#include "Acts/Propagator/Propagator.hpp"
#include "Acts/Propagator/StandardAborters.hpp"
#include "Acts/Propagator/detail/PointwiseMaterialInteraction.hpp"
#include "Acts/TrackFitting/KalmanFitterError.hpp"
#include "Acts/TrackFitting/detail/KalmanUpdateHelpers.hpp"
#include "Acts/TrackFitting/detail/VoidKalmanComponents.hpp"
#include "Acts/Utilities/CalibrationContext.hpp"
#include "Acts/Utilities/Delegate.hpp"
#include "Acts/Utilities/Logger.hpp"
#include "Acts/Utilities/Result.hpp"

#include <functional>
#include <map>
#include <memory>

namespace Acts {

/// Extension struct which holeds delegates to customize the KF behavior
template <typename traj_t>
struct KalmanFitterExtensions {
  using TrackStateProxy = typename MultiTrajectory<traj_t>::TrackStateProxy;
  using ConstTrackStateProxy =
      typename MultiTrajectory<traj_t>::ConstTrackStateProxy;
  using Parameters = typename TrackStateProxy::Parameters;

  using Calibrator = Delegate<void(const GeometryContext&, TrackStateProxy)>;

  using Smoother = Delegate<Result<void>(
      const GeometryContext&, MultiTrajectory<traj_t>&, size_t, LoggerWrapper)>;

  using Updater = Delegate<Result<void>(const GeometryContext&, TrackStateProxy,
                                        NavigationDirection, LoggerWrapper)>;

  using OutlierFinder = Delegate<bool(ConstTrackStateProxy)>;

  using ReverseFilteringLogic = Delegate<bool(ConstTrackStateProxy)>;

  /// The Calibrator is a dedicated calibration algorithm that allows
  /// to calibrate measurements using track information, this could be
  /// e.g. sagging for wires, module deformations, etc.
  Calibrator calibrator;

  /// The updater incorporates measurement information into the track parameters
  Updater updater;

  /// The smoother back-propagates measurement information along the track
  Smoother smoother;

  /// Determines whether a measurement is supposed to be considered as an
  /// outlier
  OutlierFinder outlierFinder;

  /// Decides whether the smoothing stage uses linearized transport or full
  /// reverse propagation
  ReverseFilteringLogic reverseFilteringLogic;

  /// Default constructor which connects the default void components
  KalmanFitterExtensions() {
    calibrator.template connect<&voidKalmanCalibrator<traj_t>>();
    updater.template connect<&voidKalmanUpdater<traj_t>>();
    smoother.template connect<&voidKalmanSmoother<traj_t>>();
    outlierFinder.template connect<&voidOutlierFinder<traj_t>>();
    reverseFilteringLogic
        .template connect<&voidReverseFilteringLogic<traj_t>>();
  }
};

/// Combined options for the Kalman fitter.
///
/// @tparam traj_t The trajectory type
template <typename traj_t>
struct KalmanFitterOptions {
  /// PropagatorOptions with context.
  ///
  /// @param gctx The goemetry context for this fit
  /// @param mctx The magnetic context for this fit
  /// @param cctx The calibration context for this fit
  /// @param extensions_ The KF extensions
  /// @param logger_ The logger wrapper
  /// @param pOptions The plain propagator options
  /// @param rSurface The reference surface for the fit to be expressed at
  /// @param mScattering Whether to include multiple scattering
  /// @param eLoss Whether to include energy loss
  /// @param rFiltering Whether to run filtering in reversed direction as smoothing
  /// @param rfScaling Scale factor for the covariance matrix before the backward filtering
  /// @param freeToBoundCorrection_ Correction for non-linearity effect during transform from free to bound
  KalmanFitterOptions(const GeometryContext& gctx,
                      const MagneticFieldContext& mctx,
                      std::reference_wrapper<const CalibrationContext> cctx,
                      KalmanFitterExtensions<traj_t> extensions_,
                      LoggerWrapper logger_,
                      const PropagatorPlainOptions& pOptions,
                      const Surface* rSurface = nullptr,
                      bool mScattering = true, bool eLoss = true,
                      bool rFiltering = false, double rfScaling = 1.0,
                      const FreeToBoundCorrection& freeToBoundCorrection_ =
                          FreeToBoundCorrection(false))
      : geoContext(gctx),
        magFieldContext(mctx),
        calibrationContext(cctx),
        extensions(std::move(extensions_)),
        propagatorPlainOptions(pOptions),
        referenceSurface(rSurface),
        multipleScattering(mScattering),
        energyLoss(eLoss),
        reversedFiltering(rFiltering),
        reversedFilteringCovarianceScaling(rfScaling),
        freeToBoundCorrection(freeToBoundCorrection_),
        logger(logger_) {}
  /// Contexts are required and the options must not be default-constructible.
  KalmanFitterOptions() = delete;

  /// Context object for the geometry
  std::reference_wrapper<const GeometryContext> geoContext;
  /// Context object for the magnetic field
  std::reference_wrapper<const MagneticFieldContext> magFieldContext;
  /// context object for the calibration
  std::reference_wrapper<const CalibrationContext> calibrationContext;

  KalmanFitterExtensions<traj_t> extensions;

  /// The trivial propagator options
  PropagatorPlainOptions propagatorPlainOptions;

  /// The reference Surface
  const Surface* referenceSurface = nullptr;

  /// Whether to consider multiple scattering
  bool multipleScattering = true;

  /// Whether to consider energy loss
  bool energyLoss = true;

  /// Whether to run filtering in reversed direction overwrite the
  /// ReverseFilteringLogic
  bool reversedFiltering = false;

  /// Factor by which the covariance of the input of the reversed filtering is
  /// scaled. This is only used in the backwardfiltering (if reversedFiltering
  /// is true or if the ReverseFilteringLogic return true for the track of
  /// interest)
  double reversedFilteringCovarianceScaling = 1.0;

  /// Whether to include non-linear correction during global to local
  /// transformation
  FreeToBoundCorrection freeToBoundCorrection;

  /// Logger
  LoggerWrapper logger;
};

template <typename traj_t>
struct KalmanFitterResult {
  // Fitted states that the actor has handled.
  traj_t fittedStates;

  // This is the index of the 'tip' of the track stored in multitrajectory.
  // This correspond to the last measurment state in the multitrajectory.
  // Since this KF only stores one trajectory, it is unambiguous.
  // SIZE_MAX is the start of a trajectory.
  size_t lastMeasurementIndex = SIZE_MAX;

  // This is the index of the 'tip' of the states stored in multitrajectory.
  // This correspond to the last state in the multitrajectory.
  // Since this KF only stores one trajectory, it is unambiguous.
  // SIZE_MAX is the start of a trajectory.
  size_t lastTrackIndex = SIZE_MAX;

  // The optional Parameters at the provided surface
  std::optional<BoundTrackParameters> fittedParameters;

  // Counter for states with non-outlier measurements
  size_t measurementStates = 0;

  // Counter for measurements holes
  // A hole correspond to a surface with an associated detector element with no
  // associated measurment. Holes are only taken into account if they are
  // between the first and last measurements.
  size_t measurementHoles = 0;

  // Counter for handled states
  size_t processedStates = 0;

  // Indicator if smoothing has been done.
  bool smoothed = false;

  // Indicator if navigation direction has been reversed
  bool reversed = false;

  // Indicator if track fitting has been done
  bool finished = false;

  // Measurement surfaces without hits
  std::vector<const Surface*> missedActiveSurfaces;

  // Measurement surfaces handled in both forward and
  // backward filtering
  std::vector<const Surface*> passedAgainSurfaces;

  Result<void> result{Result<void>::success()};
};

/// Kalman fitter implementation.
///
/// @tparam propagator_t Type of the propagation class
///
/// The Kalman filter contains an Actor and a Sequencer sub-class.
/// The Sequencer has to be part of the Navigator of the Propagator
/// in order to initialize and provide the measurement surfaces.
///
/// The Actor is part of the Propagation call and does the Kalman update
/// and eventually the smoothing.  Updater, Smoother and Calibrator are
/// given to the Actor for further use:
/// - The Updater is the implemented kalman updater formalism, it
///   runs via a visitor pattern through the measurements.
/// - The Smoother is called at the end of the filtering by the Actor.
///
/// Measurements are not required to be ordered for the KalmanFilter,
/// measurement ordering needs to be figured out by the navigation of
/// the propagator.
///
/// The void components are provided mainly for unit testing.
template <typename propagator_t, typename traj_t>
class KalmanFitter {
  /// The navigator type
  using KalmanNavigator = typename propagator_t::Navigator;

  /// The navigator has DirectNavigator type or not
  static constexpr bool isDirectNavigator =
      std::is_same<KalmanNavigator, DirectNavigator>::value;

 public:
  KalmanFitter(propagator_t pPropagator)
      : m_propagator(std::move(pPropagator)) {}

 private:
  /// The propgator for the transport and material update
  propagator_t m_propagator;

  /// @brief Propagator Actor plugin for the KalmanFilter
  ///
  /// @tparam parameters_t The type of parameters used for "local" parameters.
  /// @tparam calibrator_t The type of calibrator
  /// @tparam outlier_finder_t Type of the outlier finder class
  ///
  /// The KalmanActor does not rely on the measurements to be
  /// sorted along the track.
  template <typename parameters_t>
  class Actor {
   public:
    /// Broadcast the result_type
    using result_type = KalmanFitterResult<traj_t>;

    /// The target surface
    const Surface* targetSurface = nullptr;

    /// Allows retrieving measurements for a surface
    const std::map<GeometryIdentifier,
                   std::reference_wrapper<const SourceLink>>*
        inputMeasurements = nullptr;

    /// Whether to consider multiple scattering.
    bool multipleScattering = true;

    /// Whether to consider energy loss.
    bool energyLoss = true;

    /// Whether run reversed filtering
    bool reversedFiltering = false;

    // Scale the covariance before the reversed filtering
    double reversedFilteringCovarianceScaling = 1.0;

    /// Whether to include non-linear correction during global to local
    /// transformation
    FreeToBoundCorrection freeToBoundCorrection;

    /// Input MultiTrajectory
    std::shared_ptr<MultiTrajectory<traj_t>> outputStates;

    /// @brief Kalman actor operation
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param state is the mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result is the mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    void operator()(propagator_state_t& state, const stepper_t& stepper,
                    result_type& result) const {
      const auto& logger = state.options.logger;

      if (result.finished) {
        return;
      }

      ACTS_VERBOSE("KalmanFitter step");

      // Add the measurement surface as external surface to navigator.
      // We will try to hit those surface by ignoring boundary checks.
      if constexpr (not isDirectNavigator) {
        if (result.processedStates == 0) {
          for (auto measurementIt = inputMeasurements->begin();
               measurementIt != inputMeasurements->end(); measurementIt++) {
            state.navigation.externalSurfaces.insert(
                std::pair<uint64_t, GeometryIdentifier>(
                    measurementIt->first.layer(), measurementIt->first));
          }
        }
      }

      // Update:
      // - Waiting for a current surface
      auto surface = state.navigation.currentSurface;
      std::string direction =
          (state.stepping.navDir == NavigationDirection::Forward) ? "forward"
                                                                  : "backward";
      if (surface != nullptr) {
        // Check if the surface is in the measurement map
        // -> Get the measurement / calibrate
        // -> Create the predicted state
        // -> Check outlier behavior, if non-outlier:
        // -> Perform the kalman update
        // -> Fill strack state information & update stepper information

        if (not result.smoothed and not result.reversed) {
          ACTS_VERBOSE("Perform " << direction << " filter step");
          auto res = filter(surface, state, stepper, result);
          if (!res.ok()) {
            ACTS_ERROR("Error in " << direction << " filter: " << res.error());
            result.result = res.error();
          }
        }
        if (result.reversed) {
          ACTS_VERBOSE("Perform " << direction << " filter step");
          auto res = reversedFilter(surface, state, stepper, result);
          if (!res.ok()) {
            ACTS_ERROR("Error in " << direction << " filter: " << res.error());
            result.result = res.error();
          }
        }
      }

      // Finalization:
      // when all track states have been handled or the navigation is breaked,
      // reset navigation&stepping before run reversed filtering or
      // proceed to run smoothing
      if (not result.smoothed and not result.reversed) {
        if (result.measurementStates == inputMeasurements->size() or
            (result.measurementStates > 0 and
             state.navigation.navigationBreak)) {
          // Remove the missing surfaces that occur after the last measurement
          result.missedActiveSurfaces.resize(result.measurementHoles);
          // now get track state proxy for the smoothing logic
          auto trackStateProxy =
              result.fittedStates.getTrackState(result.lastMeasurementIndex);
          if (reversedFiltering ||
              extensions.reverseFilteringLogic(trackStateProxy)) {
            // Start to run reversed filtering:
            // Reverse navigation direction and reset navigation and stepping
            // state to last measurement
            ACTS_VERBOSE("Reverse navigation direction.");
            auto res = reverse(state, stepper, result);
            if (!res.ok()) {
              ACTS_ERROR("Error in reversing navigation: " << res.error());
              result.result = res.error();
            }
          } else {
            // --> Search the starting state to run the smoothing
            // --> Call the smoothing
            // --> Set a stop condition when all track states have been
            // handled
            ACTS_VERBOSE("Finalize/run smoothing");
            auto res = finalize(state, stepper, result);
            if (!res.ok()) {
              ACTS_ERROR("Error in finalize: " << res.error());
              result.result = res.error();
            }
          }
        }
      }

      // Post-finalization:
      // - Progress to target/reference surface and built the final track
      // parameters
      if (result.smoothed or result.reversed) {
        if (targetSurface == nullptr) {
          // If no target surface provided:
          // -> Return an error when using reversed filtering mode
          // -> Fitting is finished here
          if (result.reversed) {
            ACTS_ERROR(
                "The target surface needed for aborting reversed propagation "
                "is not provided");
            result.result =
                Result<void>(KalmanFitterError::BackwardUpdateFailed);
          } else {
            ACTS_VERBOSE(
                "No target surface set. Completing without fitted track "
                "parameter");
            // Remember the track fitting is done
            result.finished = true;
          }
        } else if (targetReached(state, stepper, *targetSurface)) {
          ACTS_VERBOSE("Completing with fitted track parameter");
          // Transport & bind the parameter to the final surface
          auto res = stepper.boundState(state.stepping, *targetSurface, true,
                                        freeToBoundCorrection);
          if (!res.ok()) {
            ACTS_ERROR("Error in " << direction << " filter: " << res.error());
            result.result = res.error();
            return;
          }
          auto& fittedState = *res;
          // Assign the fitted parameters
          result.fittedParameters = std::get<BoundTrackParameters>(fittedState);

          // Reset smoothed status of states missed in reversed filtering
          if (result.reversed) {
            result.fittedStates.applyBackwards(
                result.lastMeasurementIndex, [&](auto trackState) {
                  auto fSurface = &trackState.referenceSurface();
                  auto surface_it = std::find_if(
                      result.passedAgainSurfaces.begin(),
                      result.passedAgainSurfaces.end(),
                      [=](const Surface* s) { return s == fSurface; });
                  if (surface_it == result.passedAgainSurfaces.end()) {
                    // If reversed filtering missed this surface, then there is
                    // no smoothed parameter
                    trackState.unset(TrackStatePropMask::Smoothed);
                  }
                });
          }
          // Remember the track fitting is done
          result.finished = true;
        }
      }
    }

    /// @brief Kalman actor operation : reverse direction
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param state is the mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result is the mutable result state objecte
    template <typename propagator_state_t, typename stepper_t>
    Result<void> reverse(propagator_state_t& state, stepper_t& stepper,
                         result_type& result) const {
      const auto& logger = state.options.logger;

      // Check if there is a measurement on track
      if (result.lastMeasurementIndex == SIZE_MAX) {
        ACTS_ERROR("No point to reverse for a track without measurements.");
        return KalmanFitterError::ReverseNavigationFailed;
      }

      // Remember the navigation direction has been reversed
      result.reversed = true;

      // Reverse navigation direction
      state.stepping.navDir =
          (state.stepping.navDir == NavigationDirection::Forward)
              ? NavigationDirection::Backward
              : NavigationDirection::Forward;

      // Reset propagator options
      state.options.maxStepSize =
          state.stepping.navDir * std::abs(state.options.maxStepSize);
      // Not sure if reset of pathLimit during propagation makes any sense
      state.options.pathLimit =
          state.stepping.navDir * std::abs(state.options.pathLimit);

      // Get the last measurement state and reset navigation&stepping state
      // based on information on this state
      auto st = result.fittedStates.getTrackState(result.lastMeasurementIndex);

      // Update the stepping state
      stepper.resetState(
          state.stepping, st.filtered(),
          reversedFilteringCovarianceScaling * st.filteredCovariance(),
          st.referenceSurface(), state.stepping.navDir,
          state.options.maxStepSize);

      // For the last measurement state, smoothed is filtered
      st.smoothed() = st.filtered();
      st.smoothedCovariance() = st.filteredCovariance();
      result.passedAgainSurfaces.push_back(&st.referenceSurface());

      // Reset navigation state
      state.navigation.reset(state.geoContext, stepper.position(state.stepping),
                             stepper.direction(state.stepping),
                             state.stepping.navDir, &st.referenceSurface(),
                             targetSurface);

      // Update material effects for last measurement state in reversed
      // direction
      materialInteractor(state.navigation.currentSurface, state, stepper);

      return Result<void>::success();
    }

    /// @brief Kalman actor operation : update
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param surface The surface where the update happens
    /// @param state The mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result The mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    Result<void> filter(const Surface* surface, propagator_state_t& state,
                        const stepper_t& stepper, result_type& result) const {
      const auto& logger = state.options.logger;
      // Try to find the surface in the measurement surfaces
      auto sourcelink_it = inputMeasurements->find(surface->geometryId());
      if (sourcelink_it != inputMeasurements->end()) {
        // Screen output message
        ACTS_VERBOSE("Measurement surface " << surface->geometryId()
                                            << " detected.");
        // Transport the covariance to the surface
        stepper.transportCovarianceToBound(state.stepping, *surface,
                                           freeToBoundCorrection);

        // Update state and stepper with pre material effects
        materialInteractor(surface, state, stepper,
                           MaterialUpdateStage::PreUpdate);

        // do the kalman update (no need to perform covTransport here, hence no
        // point in performing globalToLocal correction)
        auto trackStateProxyRes = detail::kalmanHandleMeasurement(
            state, stepper, extensions, *surface, sourcelink_it->second,
            result.fittedStates, result.lastTrackIndex, false);

        if (!trackStateProxyRes.ok()) {
          return trackStateProxyRes.error();
        }

        const auto& trackStateProxy = *trackStateProxyRes;
        result.lastTrackIndex = trackStateProxy.index();

        // Update the stepper if it is not an outlier
        if (trackStateProxy.typeFlags().test(
                Acts::TrackStateFlag::MeasurementFlag)) {
          // Update the stepping state with filtered parameters
          ACTS_VERBOSE("Filtering step successful, updated parameters are : \n"
                       << trackStateProxy.filtered().transpose());
          // update stepping state using filtered parameters after kalman
          stepper.update(state.stepping,
                         MultiTrajectoryHelpers::freeFiltered(
                             state.options.geoContext, trackStateProxy),
                         trackStateProxy.filtered(),
                         trackStateProxy.filteredCovariance(), *surface);
          // We count the state with measurement
          ++result.measurementStates;
        }

        // Update state and stepper with post material effects
        materialInteractor(surface, state, stepper,
                           MaterialUpdateStage::PostUpdate);
        // We count the processed state
        ++result.processedStates;
        // Update the number of holes count only when encoutering a
        // measurement
        result.measurementHoles = result.missedActiveSurfaces.size();
        // Since we encountered a measurment update the lastMeasurementIndex to
        // the lastTrackIndex.
        result.lastMeasurementIndex = result.lastTrackIndex;

      } else if (surface->associatedDetectorElement() != nullptr ||
                 surface->surfaceMaterial() != nullptr) {
        // We only create track states here if there is already measurement
        // detected or if the surface has material (no holes before the first
        // measurement)
        if (result.measurementStates > 0 ||
            surface->surfaceMaterial() != nullptr) {
          auto trackStateProxyRes = detail::kalmanHandleNoMeasurement(
              state, stepper, *surface, result.fittedStates,
              result.lastTrackIndex, true, freeToBoundCorrection);

          if (!trackStateProxyRes.ok()) {
            return trackStateProxyRes.error();
          }

          const auto& trackStateProxy = *trackStateProxyRes;
          result.lastTrackIndex = trackStateProxy.index();

          if (trackStateProxy.typeFlags().test(TrackStateFlag::HoleFlag)) {
            // Count the missed surface
            result.missedActiveSurfaces.push_back(surface);
          }

          ++result.processedStates;
        }
        if (surface->surfaceMaterial() != nullptr) {
          // Update state and stepper with material effects
          materialInteractor(surface, state, stepper,
                             MaterialUpdateStage::FullUpdate);
        }
      }
      return Result<void>::success();
    }

    /// @brief Kalman actor operation : update in reversed direction
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param surface The surface where the update happens
    /// @param state The mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result The mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    Result<void> reversedFilter(const Surface* surface,
                                propagator_state_t& state,
                                const stepper_t& stepper,
                                result_type& result) const {
      const auto& logger = state.options.logger;
      // Try to find the surface in the measurement surfaces
      auto sourcelink_it = inputMeasurements->find(surface->geometryId());
      if (sourcelink_it != inputMeasurements->end()) {
        // Screen output message
        ACTS_VERBOSE("Measurement surface "
                     << surface->geometryId()
                     << " detected in reversed propagation.");

        // No reversed filtering for last measurement state, but still update
        // with material effects
        if (result.reversed and surface == state.navigation.startSurface) {
          materialInteractor(surface, state, stepper);
          return Result<void>::success();
        }

        // Transport the covariance to the surface
        stepper.transportCovarianceToBound(state.stepping, *surface,
                                           freeToBoundCorrection);

        // Update state and stepper with pre material effects
        materialInteractor(surface, state, stepper,
                           MaterialUpdateStage::PreUpdate);

        // Bind the transported state to the current surface
        auto res = stepper.boundState(state.stepping, *surface, false);
        if (!res.ok()) {
          return res.error();
        }

        auto& [boundParams, jacobian, pathLength] = *res;

        // Create a detached track state proxy
        auto tempTrackTip =
            result.fittedStates.addTrackState(TrackStatePropMask::All);

        // Get the detached track state proxy back
        auto trackStateProxy = result.fittedStates.getTrackState(tempTrackTip);

        trackStateProxy.setReferenceSurface(surface->getSharedPtr());

        // Assign the source link to the detached track state
        trackStateProxy.setUncalibrated(sourcelink_it->second);

        // Fill the track state
        trackStateProxy.predicted() = std::move(boundParams.parameters());
        if (boundParams.covariance().has_value()) {
          trackStateProxy.predictedCovariance() =
              std::move(*boundParams.covariance());
        }
        trackStateProxy.jacobian() = std::move(jacobian);
        trackStateProxy.pathLength() = std::move(pathLength);

        // We have predicted parameters, so calibrate the uncalibrated input
        // measuerement
        extensions.calibrator(state.geoContext, trackStateProxy);

        // If the update is successful, set covariance and
        auto updateRes = extensions.updater(state.geoContext, trackStateProxy,
                                            state.stepping.navDir, logger);
        if (!updateRes.ok()) {
          ACTS_ERROR("Backward update step failed: " << updateRes.error());
          return updateRes.error();
        } else {
          // Update the stepping state with filtered parameters
          ACTS_VERBOSE(
              "Backward Filtering step successful, updated parameters are : "
              "\n"
              << trackStateProxy.filtered().transpose());

          // Fill the smoothed parameter for the existing track state
          result.fittedStates.applyBackwards(
              result.lastMeasurementIndex, [&](auto trackState) {
                auto fSurface = &trackState.referenceSurface();
                if (fSurface == surface) {
                  result.passedAgainSurfaces.push_back(surface);
                  trackState.smoothed() = trackStateProxy.filtered();
                  trackState.smoothedCovariance() =
                      trackStateProxy.filteredCovariance();
                  return false;
                }
                return true;
              });

          // update stepping state using filtered parameters after kalman
          // update We need to (re-)construct a BoundTrackParameters instance
          // here, which is a bit awkward.
          stepper.update(state.stepping,
                         MultiTrajectoryHelpers::freeFiltered(
                             state.options.geoContext, trackStateProxy),
                         trackStateProxy.filtered(),
                         trackStateProxy.filteredCovariance(), *surface);

          // Update state and stepper with post material effects
          materialInteractor(surface, state, stepper,
                             MaterialUpdateStage::PostUpdate);
        }
      } else if (surface->associatedDetectorElement() != nullptr ||
                 surface->surfaceMaterial() != nullptr) {
        // Transport covariance
        if (surface->associatedDetectorElement() != nullptr) {
          ACTS_VERBOSE("Detected hole on " << surface->geometryId()
                                           << " in reversed filtering");
          if (state.stepping.covTransport) {
            stepper.transportCovarianceToBound(state.stepping, *surface);
          }
        } else if (surface->surfaceMaterial() != nullptr) {
          ACTS_VERBOSE("Detected in-sensitive surface "
                       << surface->geometryId() << " in reversed filtering");
          if (state.stepping.covTransport) {
            stepper.transportCovarianceToCurvilinear(state.stepping);
          }
        }
        // Not creating bound state here, so need manually reinitialize
        // jacobian
        stepper.setIdentityJacobian(state.stepping);
        if (surface->surfaceMaterial() != nullptr) {
          // Update state and stepper with material effects
          materialInteractor(surface, state, stepper);
        }
      }

      return Result<void>::success();
    }

    /// @brief Kalman actor operation : material interaction
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param surface The surface where the material interaction happens
    /// @param state The mutable propagator state object
    /// @param stepper The stepper in use
    /// @param updateStage The materal update stage
    ///
    template <typename propagator_state_t, typename stepper_t>
    void materialInteractor(const Surface* surface, propagator_state_t& state,
                            stepper_t& stepper,
                            const MaterialUpdateStage& updateStage =
                                MaterialUpdateStage::FullUpdate) const {
      const auto& logger = state.options.logger;
      // Indicator if having material
      bool hasMaterial = false;

      if (surface and surface->surfaceMaterial()) {
        // Prepare relevant input particle properties
        detail::PointwiseMaterialInteraction interaction(surface, state,
                                                         stepper);
        // Evaluate the material properties
        if (interaction.evaluateMaterialSlab(state, updateStage)) {
          // Surface has material at this stage
          hasMaterial = true;

          // Evaluate the material effects
          interaction.evaluatePointwiseMaterialInteraction(multipleScattering,
                                                           energyLoss);

          // Screen out material effects info
          ACTS_VERBOSE("Material effects on surface: "
                       << surface->geometryId()
                       << " at update stage: " << updateStage << " are :");
          ACTS_VERBOSE("eLoss = "
                       << interaction.Eloss << ", "
                       << "variancePhi = " << interaction.variancePhi << ", "
                       << "varianceTheta = " << interaction.varianceTheta
                       << ", "
                       << "varianceQoverP = " << interaction.varianceQoverP);

          // Update the state and stepper with material effects
          interaction.updateState(state, stepper, addNoise);
        }
      }

      if (not hasMaterial) {
        // Screen out message
        ACTS_VERBOSE("No material effects on surface: " << surface->geometryId()
                                                        << " at update stage: "
                                                        << updateStage);
      }
    }

    /// @brief Kalman actor operation : finalize
    ///
    /// @tparam propagator_state_t is the type of Propagagor state
    /// @tparam stepper_t Type of the stepper
    ///
    /// @param state is the mutable propagator state object
    /// @param stepper The stepper in use
    /// @param result is the mutable result state object
    template <typename propagator_state_t, typename stepper_t>
    Result<void> finalize(propagator_state_t& state, const stepper_t& stepper,
                          result_type& result) const {
      const auto& logger = state.options.logger;
      // Remember you smoothed the track states
      result.smoothed = true;

      // Get the indices of the first states (can be either a measurement or
      // material);
      size_t firstStateIndex = result.lastMeasurementIndex;
      // Count track states to be smoothed
      size_t nStates = 0;
      result.fittedStates.applyBackwards(
          result.lastMeasurementIndex, [&](auto st) {
            bool isMeasurement =
                st.typeFlags().test(TrackStateFlag::MeasurementFlag);
            bool isMaterial = st.typeFlags().test(TrackStateFlag::MaterialFlag);
            if (isMeasurement || isMaterial) {
              firstStateIndex = st.index();
            }
            nStates++;
          });
      // Return error if the track has no measurement states (but this should
      // not happen)
      if (nStates == 0) {
        ACTS_ERROR("Smoothing for a track without measurements.");
        return KalmanFitterError::SmoothFailed;
      }
      // Screen output for debugging
      if (logger().doPrint(Logging::VERBOSE)) {
        ACTS_VERBOSE("Apply smoothing on " << nStates
                                           << " filtered track states.");
      }

      // Smooth the track states
      auto smoothRes =
          extensions.smoother(state.geoContext, result.fittedStates,
                              result.lastMeasurementIndex, logger);
      if (!smoothRes.ok()) {
        ACTS_ERROR("Smoothing step failed: " << smoothRes.error());
        return smoothRes.error();
      }

      // Return in case no target surface
      if (targetSurface == nullptr) {
        return Result<void>::success();
      }

      // Obtain the smoothed parameters at first/last measurement state
      auto firstCreatedState =
          result.fittedStates.getTrackState(firstStateIndex);
      auto lastCreatedMeasurement =
          result.fittedStates.getTrackState(result.lastMeasurementIndex);

      // Lambda to get the intersection of the free params on the target surface
      auto target = [&](const FreeVector& freeVector) -> SurfaceIntersection {
        return targetSurface->intersect(
            state.geoContext, freeVector.segment<3>(eFreePos0),
            state.stepping.navDir * freeVector.segment<3>(eFreeDir0), true);
      };

      // The smoothed free params at the first/last measurement state.
      // (the first state can also be a material state)
      auto firstParams = MultiTrajectoryHelpers::freeSmoothed(
          state.options.geoContext, firstCreatedState);
      auto lastParams = MultiTrajectoryHelpers::freeSmoothed(
          state.options.geoContext, lastCreatedMeasurement);
      // Get the intersections of the smoothed free parameters with the target
      // surface
      const auto firstIntersection = target(firstParams);
      const auto lastIntersection = target(lastParams);

      // Update the stepping parameters - in order to progress to destination.
      // At the same time, reverse navigation direction for further
      // stepping if necessary.
      // @note The stepping parameters is updated to the smoothed parameters at
      // either the first measurement state or the last measurement state. It
      // assumes the target surface is not within the first and the last
      // smoothed measurement state. Also, whether the intersection is on
      // surface is not checked here.
      bool reverseDirection = false;
      bool closerTofirstCreatedState =
          (std::abs(firstIntersection.intersection.pathLength) <=
           std::abs(lastIntersection.intersection.pathLength));
      if (closerTofirstCreatedState) {
        stepper.resetState(state.stepping, firstCreatedState.smoothed(),
                           firstCreatedState.smoothedCovariance(),
                           firstCreatedState.referenceSurface());
        reverseDirection = (firstIntersection.intersection.pathLength < 0);
      } else {
        stepper.resetState(state.stepping, lastCreatedMeasurement.smoothed(),
                           lastCreatedMeasurement.smoothedCovariance(),
                           lastCreatedMeasurement.referenceSurface());
        reverseDirection = (lastIntersection.intersection.pathLength < 0);
      }
      const auto& surface = closerTofirstCreatedState
                                ? firstCreatedState.referenceSurface()
                                : lastCreatedMeasurement.referenceSurface();
      ACTS_VERBOSE(
          "Smoothing successful, updating stepping state to smoothed "
          "parameters at surface "
          << surface.geometryId() << ". Prepared to reach the target surface.");

      // Reverse the navigation direction if necessary
      if (reverseDirection) {
        ACTS_VERBOSE(
            "Reverse navigation direction after smoothing for reaching the "
            "target surface");
        state.stepping.navDir =
            (state.stepping.navDir == NavigationDirection::Forward)
                ? NavigationDirection::Backward
                : NavigationDirection::Forward;
      }
      // Reset the step size
      state.stepping.stepSize = ConstrainedStep(
          state.stepping.navDir * std::abs(state.options.maxStepSize));
      // Set accumulatd path to zero before targeting surface
      state.stepping.pathAccumulated = 0.;

      return Result<void>::success();
    }

    KalmanFitterExtensions<traj_t> extensions;

    /// The Surface beeing
    SurfaceReached targetReached;
  };

  template <typename parameters_t>
  class Aborter {
   public:
    /// Broadcast the result_type
    using action_type = Actor<parameters_t>;

    template <typename propagator_state_t, typename stepper_t,
              typename result_t>
    bool operator()(propagator_state_t& /*state*/, const stepper_t& /*stepper*/,
                    const result_t& result) const {
      if (!result.result.ok() or result.finished) {
        return true;
      }
      return false;
    }
  };

 public:
  /// Fit implementation of the foward filter, calls the
  /// the filter and smoother/reversed filter
  ///
  /// @tparam source_link_iterator_t Iterator type used to pass source links
  /// @tparam start_parameters_t Type of the initial parameters
  /// @tparam parameters_t Type of parameters used for local parameters
  ///
  /// @param it Begin iterator for the fittable uncalibrated measurements
  /// @param end End iterator for the fittable uncalibrated measurements
  /// @param sParameters The initial track parameters
  /// @param kfOptions KalmanOptions steering the fit
  /// @note The input measurements are given in the form of @c SourceLink s.
  /// It's the calibrators job to turn them into calibrated measurements used in
  /// the fit.
  ///
  /// @return the output as an output track
  template <typename source_link_iterator_t, typename start_parameters_t,
            typename parameters_t = BoundTrackParameters,
            bool _isdn = isDirectNavigator>
  auto fit(source_link_iterator_t it, source_link_iterator_t end,
           const start_parameters_t& sParameters,
           const KalmanFitterOptions<traj_t>& kfOptions) const
      -> std::enable_if_t<!_isdn, Result<KalmanFitterResult<traj_t>>> {
    const auto& logger = kfOptions.logger;

    // To be able to find measurements later, we put them into a map
    // We need to copy input SourceLinks anyways, so the map can own them.
    ACTS_VERBOSE("Preparing " << std::distance(it, end)
                              << " input measurements");
    std::map<GeometryIdentifier, std::reference_wrapper<const SourceLink>>
        inputMeasurements;
    // for (const auto& sl : sourcelinks) {
    for (; it != end; ++it) {
      const SourceLink& sl = *it;
      inputMeasurements.emplace(sl.geometryId(), sl);
    }

    // Create the ActionList and AbortList
    using KalmanAborter = Aborter<parameters_t>;
    using KalmanActor = Actor<parameters_t>;

    using KalmanResult = typename KalmanActor::result_type;
    using Actors = ActionList<KalmanActor>;
    using Aborters = AbortList<KalmanAborter>;

    // Create relevant options for the propagation options
    PropagatorOptions<Actors, Aborters> kalmanOptions(
        kfOptions.geoContext, kfOptions.magFieldContext, logger);

    // // Set the trivial propagator options
    kalmanOptions.setPlainOptions(kfOptions.propagatorPlainOptions);

    // Catch the actor and set the measurements
    auto& kalmanActor = kalmanOptions.actionList.template get<KalmanActor>();
    kalmanActor.inputMeasurements = &inputMeasurements;
    kalmanActor.targetSurface = kfOptions.referenceSurface;
    kalmanActor.multipleScattering = kfOptions.multipleScattering;
    kalmanActor.energyLoss = kfOptions.energyLoss;
    kalmanActor.reversedFiltering = kfOptions.reversedFiltering;
    kalmanActor.reversedFilteringCovarianceScaling =
        kfOptions.reversedFilteringCovarianceScaling;
    kalmanActor.freeToBoundCorrection =
        std::move(kfOptions.freeToBoundCorrection);
    kalmanActor.extensions = std::move(kfOptions.extensions);

    // Run the fitter
    auto result = m_propagator.template propagate(sParameters, kalmanOptions);

    if (!result.ok()) {
      ACTS_ERROR("Propapation failed: " << result.error());
      return result.error();
    }

    auto& propRes = *result;

    /// Get the result of the fit
    auto kalmanResult = std::move(propRes.template get<KalmanResult>());

    /// It could happen that the fit ends in zero measurement states.
    /// The result gets meaningless so such case is regarded as fit failure.
    if (kalmanResult.result.ok() and not kalmanResult.measurementStates) {
      kalmanResult.result = Result<void>(KalmanFitterError::NoMeasurementFound);
    }

    if (!kalmanResult.result.ok()) {
      ACTS_ERROR("KalmanFilter failed: "
                 << kalmanResult.result.error() << ", "
                 << kalmanResult.result.error().message());
      return kalmanResult.result.error();
    }

    // Return the converted Track
    return kalmanResult;
  }

  /// Fit implementation of the foward filter, calls the
  /// the filter and smoother/reversed filter
  ///
  /// @tparam source_link_iterator_t Iterator type used to pass source links
  /// @tparam start_parameters_t Type of the initial parameters
  /// @tparam parameters_t Type of parameters used for local parameters
  ///
  /// @param it Begin iterator for the fittable uncalibrated measurements
  /// @param end End iterator for the fittable uncalibrated measurements
  /// @param sParameters The initial track parameters
  /// @param kfOptions KalmanOptions steering the fit
  /// @param sSequence surface sequence used to initialize a DirectNavigator
  /// @note The input measurements are given in the form of @c SourceLinks.
  /// It's
  /// @c calibrator_t's job to turn them into calibrated measurements used in
  /// the fit.
  ///
  /// @return the output as an output track
  template <typename source_link_iterator_t, typename start_parameters_t,
            typename parameters_t = BoundTrackParameters,
            bool _isdn = isDirectNavigator>
  auto fit(source_link_iterator_t it, source_link_iterator_t end,
           const start_parameters_t& sParameters,
           const KalmanFitterOptions<traj_t>& kfOptions,
           const std::vector<const Surface*>& sSequence) const
      -> std::enable_if_t<_isdn, Result<KalmanFitterResult<traj_t>>> {
    const auto& logger = kfOptions.logger;

    // To be able to find measurements later, we put them into a map
    // We need to copy input SourceLinks anyways, so the map can own them.
    ACTS_VERBOSE("Preparing " << std::distance(it, end)
                              << " input measurements");
    std::map<GeometryIdentifier, std::reference_wrapper<const SourceLink>>
        inputMeasurements;
    for (; it != end; ++it) {
      const SourceLink& sl = *it;
      inputMeasurements.emplace(sl.geometryId(), sl);
    }

    // Create the ActionList and AbortList
    using KalmanAborter = Aborter<parameters_t>;
    using KalmanActor = Actor<parameters_t>;

    using KalmanResult = typename KalmanActor::result_type;
    using Actors = ActionList<DirectNavigator::Initializer, KalmanActor>;
    using Aborters = AbortList<KalmanAborter>;

    // Create relevant options for the propagation options
    PropagatorOptions<Actors, Aborters> kalmanOptions(
        kfOptions.geoContext, kfOptions.magFieldContext, logger);

    // Set the trivial propagator options
    kalmanOptions.setPlainOptions(kfOptions.propagatorPlainOptions);

    // Catch the actor and set the measurements
    auto& kalmanActor = kalmanOptions.actionList.template get<KalmanActor>();
    kalmanActor.inputMeasurements = &inputMeasurements;
    kalmanActor.targetSurface = kfOptions.referenceSurface;
    kalmanActor.multipleScattering = kfOptions.multipleScattering;
    kalmanActor.energyLoss = kfOptions.energyLoss;
    kalmanActor.reversedFiltering = kfOptions.reversedFiltering;
    kalmanActor.reversedFilteringCovarianceScaling =
        kfOptions.reversedFilteringCovarianceScaling;
    kalmanActor.extensions = std::move(kfOptions.extensions);

    // Set the surface sequence
    auto& dInitializer =
        kalmanOptions.actionList.template get<DirectNavigator::Initializer>();
    dInitializer.navSurfaces = sSequence;

    // Run the fitter
    auto result = m_propagator.template propagate(sParameters, kalmanOptions);

    if (!result.ok()) {
      ACTS_ERROR("Propapation failed: " << result.error());
      return result.error();
    }

    const auto& propRes = *result;

    /// Get the result of the fit
    auto kalmanResult = propRes.template get<KalmanResult>();

    /// It could happen that the fit ends in zero measurement states.
    /// The result gets meaningless so such case is regarded as fit failure.
    if (kalmanResult.result.ok() and not kalmanResult.measurementStates) {
      kalmanResult.result = Result<void>(KalmanFitterError::NoMeasurementFound);
    }

    if (!kalmanResult.result.ok()) {
      ACTS_ERROR("KalmanFilter failed: "
                 << kalmanResult.result.error() << ", "
                 << kalmanResult.result.error().message());
      return kalmanResult.result.error();
    }

    // Return the converted Track
    return kalmanResult;
  }
};

}  // namespace Acts
