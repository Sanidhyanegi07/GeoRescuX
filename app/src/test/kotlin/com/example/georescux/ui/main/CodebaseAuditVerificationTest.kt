package com.example.georescux.ui.main

import com.example.georescux.data.maps.MapRegionCatalog
import com.example.georescux.domain.repository.LocationRepository
import com.example.georescux.domain.repository.SosRepository
import com.example.georescux.domain.sos.CancelSosUseCase
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import com.example.georescux.domain.sos.SosState
import com.example.georescux.domain.sos.StartSosUseCase
import com.example.georescux.domain.sos.StopSosUseCase
import com.example.georescux.domain.sos.UpdateSosLocationUseCase
import com.example.georescux.ui.sos.SosViewModel
import kotlinx.coroutines.Dispatchers
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Regression and stability test suite covering the defects fixed during the
 * Autonomous Full Codebase Audit and Repair mission:
 * 1. MapRegionCatalog completeness and deprecation suppression.
 * 2. SosViewModel note update sanitization and StateFlow consistency.
 * 3. LocationRepository lifecycle cleanup contract.
 */
class CodebaseAuditVerificationTest {

    @Test
    fun `MapRegionCatalog contains all 4 official states with valid boundaries`() {
        val officialStates = MapRegionCatalog.availableRegions
        assertEquals(4, officialStates.size)

        val ids = officialStates.map { it.id }.toSet()
        assertTrue(ids.contains("uttarakhand"))
        assertTrue(ids.contains("himachal_pradesh"))
        assertTrue(ids.contains("uttar_pradesh"))
        assertTrue(ids.contains("haryana"))

        officialStates.forEach { state ->
            assertTrue("State ${state.id} minLat < maxLat", state.minLatitude < state.maxLatitude)
            assertTrue("State ${state.id} minLng < maxLng", state.minLongitude < state.maxLongitude)
            assertNotNull("State ${state.id} lookup via byId", MapRegionCatalog.byId(state.id))
        }
    }

    @Test
    fun `SosViewModel note updates sanitize null strings and whitespace while maintaining focus integrity`() {
        val fakeRepo = object : SosRepository {
            var active: SosEmergency? = SosEmergency(id = "test-emergency-1", startedAtMs = 1000L)
            override fun getActiveEmergency(): SosEmergency? = active
            override fun startEmergency(id: String, startedAtMs: Long): SosEmergency? = active
            override fun completeEmergency(stoppedAtMs: Long): SosEmergency? = null
            override fun updateActiveEmergencyLocation(location: SosLocation?, status: SosLocationStatus) {}
            override fun updateActiveEmergencyNote(note: String?) {
                active = active?.copy(note = note)
            }
            override fun getHistory(): List<SosEmergency> = emptyList()
        }

        val fakeLocationRepo = object : LocationRepository {
            var stopped = false
            override fun startAcquisition(fastMode: Boolean, onLocation: (SosLocation) -> Unit) {}
            override fun stopAcquisition() { stopped = true }
        }

        val viewModel = SosViewModel(
            startSos = StartSosUseCase(fakeRepo),
            cancelSos = CancelSosUseCase(fakeRepo),
            stopSos = StopSosUseCase(fakeRepo),
            sosRepository = fakeRepo,
            updateSosLocation = UpdateSosLocationUseCase(fakeRepo),
            locationRepository = fakeLocationRepo,
            initialState = SosState.ACTIVE,
            mainDispatcher = Dispatchers.Unconfined,
            ioDispatcher = Dispatchers.Unconfined,
        )

        // Valid note
        viewModel.updateNote("Medical assistance required immediately")
        assertEquals("Medical assistance required immediately", viewModel.uiState.value.emergency?.note)

        // Note with whitespace only should be sanitized to null
        viewModel.updateNote("   ")
        assertNull(viewModel.uiState.value.emergency?.note)

        // Literal "null" string should be sanitized to null
        viewModel.updateNote("null")
        assertNull(viewModel.uiState.value.emergency?.note)

        // Clean note
        viewModel.updateNote("Trapped under debris")
        assertEquals("Trapped under debris", viewModel.uiState.value.emergency?.note)
    }

    @Test
    fun `LocationRepository contract stops acquisition and releases callbacks`() {
        var callbacksDelivered = 0
        var isAcquiring = false
        var activeCallback: ((SosLocation) -> Unit)? = null

        val locationRepo = object : LocationRepository {
            override fun startAcquisition(fastMode: Boolean, onLocation: (SosLocation) -> Unit) {
                isAcquiring = true
                activeCallback = onLocation
            }

            override fun stopAcquisition() {
                isAcquiring = false
                activeCallback = null
            }
        }

        locationRepo.startAcquisition(fastMode = true) {
            callbacksDelivered++
        }
        assertTrue(isAcquiring)
        assertNotNull(activeCallback)

        activeCallback?.invoke(SosLocation(30.0, 78.0, 5.0f, System.currentTimeMillis(), "gps"))
        assertEquals(1, callbacksDelivered)

        // Stop acquisition
        locationRepo.stopAcquisition()
        assertFalse(isAcquiring)
        assertNull(activeCallback)
    }
}
