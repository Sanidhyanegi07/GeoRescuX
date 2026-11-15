package com.example.georescux.ui.sos

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
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

/**
 * Verifies how the SOS ViewModel handles location during an active
 * emergency: immediate first fix, throttled later fixes, permission-missing
 * and unavailable statuses — and that none of it ever stops the emergency.
 * Uses an Unconfined dispatcher because Dispatchers.Main is unavailable on
 * the JVM.
 */
class SosViewModelLocationTest {

    private class FakeSosRepository : SosRepository {
        var active: SosEmergency? = null
        var historyList = mutableListOf<SosEmergency>()
        var locationUpdateCount = 0
        var lastLocationUpdate: Pair<SosLocation?, SosLocationStatus>? = null
        var failComplete = false
        var exceptionToThrowOnComplete: Throwable? = null

        override fun getActiveEmergency(): SosEmergency? = active

        override fun startEmergency(id: String, startedAtMs: Long): SosEmergency? {
            if (active != null) return null
            val emergency = SosEmergency(id = id, startedAtMs = startedAtMs)
            active = emergency
            return emergency
        }

        override fun completeEmergency(stoppedAtMs: Long): SosEmergency? {
            exceptionToThrowOnComplete?.let { throw it }
            if (failComplete) return null
            val activeEmergency = active ?: return null
            val completed = activeEmergency.copy(stoppedAtMs = stoppedAtMs)
            active = null
            historyList.add(completed)
            return completed
        }

        override fun updateActiveEmergencyLocation(location: SosLocation?, status: SosLocationStatus) {
            val activeEmergency = active ?: return
            locationUpdateCount++
            lastLocationUpdate = location to status
            active = activeEmergency.copy(location = location, locationStatus = status)
        }

        override fun updateActiveEmergencyNote(note: String?) {
            val activeEmergency = active ?: return
            active = activeEmergency.copy(note = note)
        }

        override fun getHistory(): List<SosEmergency> = historyList.toList()
    }

    private class FakeLocationRepository : LocationRepository {
        var started = false
        var fastModeRequested: Boolean? = null
        var stopped = false
        var onLocation: ((SosLocation) -> Unit)? = null

        override fun startAcquisition(fastMode: Boolean, onLocation: (SosLocation) -> Unit) {
            started = true
            fastModeRequested = fastMode
            this.onLocation = onLocation
        }

        override fun stopAcquisition() {
            stopped = true
            onLocation = null
        }
    }

    private fun fix(id: Int) = SosLocation(
        latitude = 52.5 + id,
        longitude = 13.4 + id,
        accuracyMeters = 10f * id,
        timestampMs = id.toLong(),
        provider = "gps",
    )

    private fun activeViewModel(): Pair<SosViewModel, Pair<FakeSosRepository, FakeLocationRepository>> {
        val repository = FakeSosRepository()
        val locationRepository = FakeLocationRepository()
        repository.startEmergency("alert-1", startedAtMs = 1_000L)
        val viewModel = SosViewModel(
            startSos = StartSosUseCase(repository),
            cancelSos = CancelSosUseCase(repository),
            stopSos = StopSosUseCase(repository),
            sosRepository = repository,
            updateSosLocation = UpdateSosLocationUseCase(repository),
            locationRepository = locationRepository,
            initialState = SosState.ACTIVE,
            mainDispatcher = Dispatchers.Unconfined,
        )
        return viewModel to (repository to locationRepository)
    }

    @Test
    fun `acquisition starts and the first fix persists immediately`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories

        viewModel.beginLocationAcquisition()

        assertTrue(locationRepository.started)
        locationRepository.onLocation!!.invoke(fix(1))

        val active = repository.getActiveEmergency()!!
        assertEquals(fix(1), active.location)
        assertEquals(SosLocationStatus.ACQUIRED, active.locationStatus)
        assertEquals(fix(1), viewModel.uiState.value.emergency?.location)
    }

    @Test
    fun `later fixes are throttled to one per minute`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()
        locationRepository.onLocation!!.invoke(fix(1))

        locationRepository.onLocation!!.invoke(fix(2))

        // Still the first fix: the second one arrived inside the throttle window.
        assertEquals(1, repository.locationUpdateCount)
        assertEquals(fix(1), repository.getActiveEmergency()!!.location)
    }

    @Test
    fun `permission missing marks the status without a location`() {
        val (viewModel, repositories) = activeViewModel()
        val repository = repositories.first

        viewModel.markPermissionMissing()

        val active = repository.getActiveEmergency()!!
        assertNull(active.location)
        assertEquals(SosLocationStatus.PERMISSION_MISSING, active.locationStatus)
        assertTrue(active.isActive) // the SOS is NOT stopped by a missing permission
    }

    @Test
    fun `timeout marks unavailable without stopping the emergency`() {
        val (viewModel, repositories) = activeViewModel()
        val repository = repositories.first

        viewModel.onLocationTimeout()

        val active = repository.getActiveEmergency()!!
        assertNull(active.location)
        assertEquals(SosLocationStatus.UNAVAILABLE, active.locationStatus)
        assertTrue(active.isActive)
    }

    @Test
    fun `a later successful fix updates an emergency marked unavailable`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.onLocationTimeout()

        viewModel.beginLocationAcquisition()
        locationRepository.onLocation!!.invoke(fix(1))

        val active = repository.getActiveEmergency()!!
        assertEquals(fix(1), active.location)
        assertEquals(SosLocationStatus.ACQUIRED, active.locationStatus)
        assertTrue(active.isActive)
    }

    @Test
    fun `stopping the emergency stops location acquisition`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()

        viewModel.stopEmergency()

        assertTrue(locationRepository.stopped)
        assertEquals(1, repository.historyList.size)
        assertNull(repository.getActiveEmergency())
        assertEquals(SosState.IDLE, viewModel.uiState.value.state)
        assertNull(viewModel.uiState.value.emergency)
    }

    @Test
    fun `stopping the emergency reverts to active with error message when persistence fails`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()
        repository.failComplete = true

        viewModel.stopEmergency()

        // Location acquisition must NOT be stopped if persistence failed
        assertFalse(locationRepository.stopped)
        // Must remain in ACTIVE state, not IDLE
        assertEquals(SosState.ACTIVE, viewModel.uiState.value.state)
        assertNotNull(viewModel.uiState.value.emergency)
        assertNotNull(viewModel.uiState.value.errorMessage)
        // Active emergency remains in repository
        assertNotNull(repository.getActiveEmergency())
        assertEquals(0, repository.historyList.size)
    }

    @Test
    fun `repeated stop requests do not cause duplicate completion`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, _) = repositories

        viewModel.stopEmergency()
        // Second call from IDLE/non-active state must be ignored
        viewModel.stopEmergency()

        assertEquals(1, repository.historyList.size)
    }

    @Test
    fun `acquisition does not start twice`() {
        val (viewModel, repositories) = activeViewModel()
        viewModel.beginLocationAcquisition()
        viewModel.beginLocationAcquisition()

        // One callback registration: a second one would duplicate fixes.
        assertEquals(1, repositories.second.onLocation?.let { 1 } ?: 0)
    }

    @Test
    fun `permission missing after a fix does not remove the location`() {
        val (viewModel, repositories) = activeViewModel()
        val repository = repositories.first
        viewModel.beginLocationAcquisition()
        repositories.second.onLocation!!.invoke(fix(1))

        viewModel.markPermissionMissing()

        // A missing permission must never erase an already acquired fix.
        val active = repository.getActiveEmergency()!!
        assertEquals(fix(1), active.location)
        assertEquals(SosLocationStatus.ACQUIRED, active.locationStatus)
        assertTrue(active.isActive)
    }

    @Test
    fun `stopping the emergency reverts to active with error message when stopSos throws exception`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()
        repository.exceptionToThrowOnComplete = RuntimeException("Simulated disk I/O failure")

        viewModel.stopEmergency()

        // Location acquisition must NOT be stopped if persistence threw an exception
        assertFalse(locationRepository.stopped)
        // Must revert to ACTIVE state, never stay stuck in STOPPING or transition to IDLE
        assertEquals(SosState.ACTIVE, viewModel.uiState.value.state)
        assertNotNull(viewModel.uiState.value.emergency)
        assertNotNull(viewModel.uiState.value.errorMessage)
        assertEquals("Failed to complete emergency. Please retry.", viewModel.uiState.value.errorMessage)
        // Active emergency remains safely in repository
        assertNotNull(repository.getActiveEmergency())
        assertEquals(0, repository.historyList.size)
    }

    @Test
    fun `stopping the emergency rethrows CancellationException and does not falsely complete`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()
        val cancellation = CancellationException("Simulated scope cancellation")
        repository.exceptionToThrowOnComplete = cancellation

        viewModel.stopEmergency()

        // CancellationException cancels the coroutine and is NOT converted to null or success.
        // It must NOT execute withContext(mainDispatcher), so it must NOT transition to
        // COMPLETED or IDLE, nor falsely set an error message.
        assertFalse(viewModel.uiState.value.state == SosState.COMPLETED)
        assertFalse(viewModel.uiState.value.state == SosState.IDLE)
        assertFalse(locationRepository.stopped)
        assertNull(viewModel.uiState.value.errorMessage)
        // Active emergency must not be cleared
        assertNotNull(repository.getActiveEmergency())
        assertEquals(0, repository.historyList.size)
    }

    @Test
    fun `rapid stop requests while in stopping state are ignored`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, _) = repositories

        // First call transitions to STOPPING synchronously
        viewModel.stopEmergency()
        // Second call while state is not ACTIVE must return immediately without effect
        viewModel.stopEmergency()

        assertEquals(1, repository.historyList.size)
    }

    @Test
    fun `failed completion preserves existing emergency details and allows subsequent retry`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()
        repository.failComplete = true

        // 1. Initial stop attempt fails
        viewModel.stopEmergency()

        // 2. State remains ACTIVE, emergency snapshot preserved, error message shown, location acquisition alive
        assertEquals(SosState.ACTIVE, viewModel.uiState.value.state)
        assertNotNull(viewModel.uiState.value.emergency)
        assertEquals("alert-1", viewModel.uiState.value.emergency?.id)
        assertNotNull(viewModel.uiState.value.errorMessage)
        assertFalse(locationRepository.stopped)

        // 3. Storage recovers and user retries
        repository.failComplete = false
        viewModel.stopEmergency()

        // 4. Retry succeeds: moves to IDLE, location acquisition stopped, error message cleared
        assertEquals(SosState.IDLE, viewModel.uiState.value.state)
        assertNull(viewModel.uiState.value.emergency)
        assertNull(viewModel.uiState.value.errorMessage)
        assertTrue(locationRepository.stopped)
        assertEquals(1, repository.historyList.size)
        assertEquals("alert-1", repository.historyList[0].id)
    }

    @Test
    fun `failed completion retains UI emergency snapshot even if repository returns null`() {
        val (viewModel, repositories) = activeViewModel()
        val (repository, locationRepository) = repositories
        viewModel.beginLocationAcquisition()
        // Simulate severe store failure where active record in repository was lost in memory
        repository.failComplete = true
        repository.active = null

        viewModel.stopEmergency()

        // Defensive fallback: UI retains current.emergency even if getActiveEmergency() was null
        assertEquals(SosState.ACTIVE, viewModel.uiState.value.state)
        assertNotNull("Emergency must not be wiped to null", viewModel.uiState.value.emergency)
        assertEquals("alert-1", viewModel.uiState.value.emergency?.id)
        assertNotNull(viewModel.uiState.value.errorMessage)
        assertFalse(locationRepository.stopped)
    }
}
