#include "S315_5313.h"

//----------------------------------------------------------------------------------------------------------------------
//##TODO## Our new colour values are basically correct, assuming what is suspected after
// analysis posted on SpritesMind, that the Mega Drive VDP never actually outputs at full
// intensity. We haven't taken the apparent "ladder effect" into account however. It is
// highly recommended that we perform our own tests on the hardware, and make some
// comparisons between captured video output from the real system, and the output from our
// emulator, when playing back on the same physical screen. If the ladder effect is real
// and does have an effect on the way the intensity is perceived on the screen, we should
// emulate it. We also need to confirm the maximum intensity output by the VDP. A step size
// of 18 for example would get a max value of 252, which would be more logical.
// const unsigned char S315_5313::paletteEntryTo8Bit[8] = {0, 36, 73, 109, 146, 182, 219, 255};
// const unsigned char S315_5313::paletteEntryTo8BitShadow[8] = {0, 18, 37, 55, 73, 91, 110, 128};
// const unsigned char S315_5313::paletteEntryTo8BitHighlight[8] = {128, 146, 165, 183, 201, 219, 238, 255};
const unsigned char S315_5313::PaletteEntryTo8Bit[8] = {0, 34, 68, 102, 136, 170, 204, 238};
const unsigned char S315_5313::PaletteEntryTo8BitShadow[8] = {0, 17, 34, 51, 68, 85, 102, 119};
const unsigned char S315_5313::PaletteEntryTo8BitHighlight[8] = {119, 136, 153, 170, 187, 204, 221, 238};

//----------------------------------------------------------------------------------------------------------------------
// External buffer functions
//----------------------------------------------------------------------------------------------------------------------
void S315_5313::LockExternalBuffers()
{
	_externalReferenceLock.ObtainReadLock();
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::UnlockExternalBuffers()
{
	_externalReferenceLock.ReleaseReadLock();
}

//----------------------------------------------------------------------------------------------------------------------
ITimedBufferInt* S315_5313::GetVRAMBuffer() const
{
	return _vram;
}

//----------------------------------------------------------------------------------------------------------------------
ITimedBufferInt* S315_5313::GetCRAMBuffer() const
{
	return _cram;
}

//----------------------------------------------------------------------------------------------------------------------
ITimedBufferInt* S315_5313::GetVSRAMBuffer() const
{
	return _vsram;
}

//----------------------------------------------------------------------------------------------------------------------
ITimedBufferInt* S315_5313::GetSpriteCacheBuffer() const
{
	return _spriteCache;
}

//----------------------------------------------------------------------------------------------------------------------
// Image buffer functions
//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::GetImageLastRenderedFrameToken() const
{
	return _lastRenderedFrameToken;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::GetImageCompletedBufferPlaneNo() const
{
	unsigned int displayingImageBufferPlane = _videoSingleBuffering? _drawingImageBufferPlane: ((_drawingImageBufferPlane + ImageBufferPlanes) - 1) % ImageBufferPlanes;
	return displayingImageBufferPlane;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::GetImageDrawingBufferPlaneNo() const
{
	return _drawingImageBufferPlane;
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::LockImageBufferData(unsigned int planeNo)
{
	_imageBufferLock[planeNo].ObtainReadLock();
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::UnlockImageBufferData(unsigned int planeNo)
{
	_imageBufferLock[planeNo].ReleaseReadLock();
}

//----------------------------------------------------------------------------------------------------------------------
const unsigned char* S315_5313::GetImageBufferData(unsigned int planeNo) const
{
	return &_imageBuffer[planeNo][0];
}

//----------------------------------------------------------------------------------------------------------------------
const S315_5313::ImageBufferInfo* S315_5313::GetImageBufferInfo(unsigned int planeNo) const
{
	return &_imageBufferInfo[planeNo][0];
}

//----------------------------------------------------------------------------------------------------------------------
const S315_5313::ImageBufferInfo* S315_5313::GetImageBufferInfo(unsigned int planeNo, unsigned int lineNo, unsigned int pixelNo) const
{
	if ((lineNo >= ImageBufferHeight) || (pixelNo >= ImageBufferWidth))
	{
		return 0;
	}
	unsigned int index = (lineNo * ImageBufferWidth) + pixelNo;
	return &_imageBufferInfo[planeNo][index];
}

//----------------------------------------------------------------------------------------------------------------------
bool S315_5313::GetImageBufferOddInterlaceFrame(unsigned int planeNo) const
{
	return _imageBufferOddInterlaceFrame[planeNo];
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::GetImageBufferLineCount(unsigned int planeNo) const
{
	return _imageBufferLineCount[planeNo];
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::GetImageBufferLineWidth(unsigned int planeNo, unsigned int lineNo) const
{
	return _imageBufferLineWidth[planeNo][lineNo];
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::GetImageBufferActiveScanPosX(unsigned int planeNo, unsigned int lineNo, unsigned int& startPosX, unsigned int& endPosX) const
{
	startPosX = _imageBufferActiveScanPosXStart[planeNo][lineNo];
	endPosX = _imageBufferActiveScanPosXEnd[planeNo][lineNo];
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::GetImageBufferActiveScanPosY(unsigned int planeNo, unsigned int& startPosY, unsigned int& endPosY) const
{
	startPosY = _imageBufferActiveScanPosYStart[planeNo];
	endPosY = _imageBufferActiveScanPosYEnd[planeNo];
}

//----------------------------------------------------------------------------------------------------------------------
// Rendering functions
//----------------------------------------------------------------------------------------------------------------------
void S315_5313::RenderThread()
{
	std::unique_lock<std::mutex> lock(_renderThreadMutex);

	// Start the render loop
	bool done = false;
	while (!done)
	{
		// Obtain a copy of the latest completed timeslice period
		bool renderTimesliceObtained = false;
		TimesliceRenderInfo timesliceRenderInfo;
		{
			std::unique_lock<std::mutex> timesliceLock(_timesliceMutex);

			// If there is at least one render timeslice pending, grab it from the queue.
			//##FIX## Our pendingRenderOperationCount doesn't really work the way we want.
			// Currently, here in the render thread, we decrement it once for each
			// timeslice received, but we don't increment it once for each timeslice.
			//##FIX## This problem with pendingRenderOperationCount is now breaking our
			// PSG and YM2612 cores, since the render thread marks itself as lagging when
			// it's not, and the execute thread waits forever for it to fix itself. We need
			// to get this working, and replicate it in the PSG and YM2612 cores.
//			if ((pendingRenderOperationCount > 0) && !regTimesliceList.empty() && !vramTimesliceList.empty() && !cramTimesliceList.empty() && !vsramTimesliceList.empty())
			if (!_regTimesliceList.empty() && !_vramTimesliceList.empty() && !_cramTimesliceList.empty() && !_vsramTimesliceList.empty() && !_spriteCacheTimesliceList.empty())
			{
				// Update the lagging state for the render thread
				--_pendingRenderOperationCount;
				_renderThreadLagging = (_pendingRenderOperationCount > maxPendingRenderOperationCount);
				_renderThreadLaggingStateChange.notify_all();

				// Grab the next completed timeslice from the timeslice list
				timesliceRenderInfo = *_timesliceRenderInfoList.begin();
				_regTimesliceCopy = *_regTimesliceList.begin();
				_vramTimesliceCopy = *_vramTimesliceList.begin();
				_cramTimesliceCopy = *_cramTimesliceList.begin();
				_vsramTimesliceCopy = *_vsramTimesliceList.begin();
				_spriteCacheTimesliceCopy = *_spriteCacheTimesliceList.begin();
				_timesliceRenderInfoList.pop_front();
				_regTimesliceList.pop_front();
				_vramTimesliceList.pop_front();
				_cramTimesliceList.pop_front();
				_vsramTimesliceList.pop_front();
				_spriteCacheTimesliceList.pop_front();

				// Flag that we managed to obtain a render timeslice
				renderTimesliceObtained = true;
			}
		}

		// If no render timeslice was available, we need to wait for a thread suspension
		// request or a new timeslice to be received, then begin the loop again.
		if (!renderTimesliceObtained)
		{
			// If the render thread has not already been instructed to stop, suspend this
			// thread until a timeslice becomes available or this thread is instructed to
			// stop.
			if (_renderThreadActive)
			{
				_renderThreadUpdate.wait(lock);
			}

			// If the render thread has been suspended, flag that we need to exit this
			// render loop.
			done = !_renderThreadActive;

			// Begin the loop again
			continue;
		}

		// Begin advance sessions for each of our timed buffers
		_reg.BeginAdvanceSession(_regSession, _regTimesliceCopy, false);
		_vram->BeginAdvanceSession(_vramSession, _vramTimesliceCopy, false);
		_cram->BeginAdvanceSession(_cramSession, _cramTimesliceCopy, true);
		_vsram->BeginAdvanceSession(_vsramSession, _vsramTimesliceCopy, false);
		_spriteCache->BeginAdvanceSession(_spriteCacheSession, _spriteCacheTimesliceCopy, false);

		// Calculate the number of cycles to advance in this update step, and reset the
		// current advance progress through this timeslice.
		unsigned int mclkCyclesToAdvance = timesliceRenderInfo.timesliceEndPosition - timesliceRenderInfo.timesliceStartPosition;
		_renderDigitalMclkCycleProgress = timesliceRenderInfo.timesliceStartPosition;

		if (!_videoDisableRenderOutput)
		{
			//##DEBUG##
			if (_outputTimingDebugMessages)
			{
				std::wcout << "VDPRenderThreadAdvance(Before): " << _renderDigitalHCounterPos << '\t' << _renderDigitalVCounterPos << '\t' << _renderDigitalMclkCycleProgress << '\t' << _renderDigitalRemainingMclkCycles << '\t' << timesliceRenderInfo.timesliceEndPosition << '\t' << mclkCyclesToAdvance << '\t' << _renderDigitalScreenModeRS0Active << '\t' << _renderDigitalScreenModeRS1Active << '\n';
			}

			// Advance the digital render process
			AdvanceRenderProcess(mclkCyclesToAdvance);

			//##DEBUG##
			if (_outputTimingDebugMessages)
			{
				std::wcout << "VDPRenderThreadAdvance(After): " << _renderDigitalHCounterPos << '\t' << _renderDigitalVCounterPos << '\t' << _renderDigitalMclkCycleProgress << '\t' << _renderDigitalRemainingMclkCycles << '\t' << timesliceRenderInfo.timesliceEndPosition << '\t' << mclkCyclesToAdvance << '\t' << _renderDigitalScreenModeRS0Active << '\t' << _renderDigitalScreenModeRS1Active << '\n';
			}
		}

		// Advance past the timeslice we've just rendered from
		{
			//##TODO## I don't think we need this lock here anymore. Confirm that we can
			// remove it.
			std::unique_lock<std::mutex> timesliceLock(_timesliceMutex);
			_reg.AdvancePastTimeslice(_regTimesliceCopy);
			_vram->AdvancePastTimeslice(_vramTimesliceCopy);
			_cram->AdvancePastTimeslice(_cramTimesliceCopy);
			_vsram->AdvancePastTimeslice(_vsramTimesliceCopy);
			_spriteCache->AdvancePastTimeslice(_spriteCacheTimesliceCopy);
			_vram->FreeTimesliceReference(_vramTimesliceCopy);
			_cram->FreeTimesliceReference(_cramTimesliceCopy);
			_vsram->FreeTimesliceReference(_vsramTimesliceCopy);
			_spriteCache->FreeTimesliceReference(_spriteCacheTimesliceCopy);
		}
	}
	_renderThreadStopped.notify_all();
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::AdvanceRenderProcess(unsigned int mclkCyclesRemainingToAdvance)
{
	// Create our reg buffer access target. Since we're working with the committed state,
	// access will be very fast, so there's no need to worry about caching settings.
	AccessTarget accessTarget;
	accessTarget.AccessCommitted();

	// Get the hscan and vscan settings for this scanline
	const HScanSettings* hscanSettings = &GetHScanSettings(_renderDigitalScreenModeRS0Active, _renderDigitalScreenModeRS1Active);
	const VScanSettings* vscanSettings = &GetVScanSettings(_renderDigitalScreenModeV30Active, _renderDigitalPalModeActive, _renderDigitalInterlaceEnabledActive);

	// Combine any remaining mclk cycles from the previous render update step into this
	// update step.
	mclkCyclesRemainingToAdvance += _renderDigitalRemainingMclkCycles;

	// Advance until we've consumed all update cycles
	//##FIX## This loop is consuming an enormous amount of time, because we only step by a
	// single pixel clock cycle at a time. Is there any way we can do better than this?
	while (mclkCyclesRemainingToAdvance > 0)
	{
		// Advance the register buffer up to the current time. Register changes can occur
		// at any time, so we need to ensure this buffer is always current.
		_reg.AdvanceBySession(_renderDigitalMclkCycleProgress, _regSession, _regTimesliceCopy);

		// If we've reached a point where horizontal screen mode settings need to be
		// latched, cache the new settings now.
		if (_renderDigitalHCounterPos == hscanSettings->hblankSetPoint)
		{
			// Cache the new settings
			_renderDigitalScreenModeRS0Active = RegGetRS0(accessTarget);
			_renderDigitalScreenModeRS1Active = RegGetRS1(accessTarget);

			// Latch updated screen mode settings
			hscanSettings = &GetHScanSettings(_renderDigitalScreenModeRS0Active, _renderDigitalScreenModeRS1Active);
		}

		// If we've reached a point where vertical screen mode settings need to be latched,
		// cache the new settings now.
		if ((_renderDigitalVCounterPos == vscanSettings->vblankSetPoint) && (_renderDigitalHCounterPos == hscanSettings->vcounterIncrementPoint))
		{
			// Cache the new settings
			_renderDigitalScreenModeV30Active = RegGetM3(accessTarget);
			_renderDigitalInterlaceEnabledActive = RegGetLSM0(accessTarget);
			_renderDigitalInterlaceDoubleActive = RegGetLSM1(accessTarget);
			//##FIX## This is incorrect. This is retrieving the current live state of the
			// line. We need to store history information for this line, so that the
			// correct line state can be set here.
			_renderDigitalPalModeActive = _palMode;

			// Latch updated screen mode settings
			vscanSettings = &GetVScanSettings(_renderDigitalScreenModeV30Active, _renderDigitalPalModeActive, _renderDigitalInterlaceEnabledActive);
		}

		// Calculate the number of mclk cycles required to advance the render process one
		// pixel clock step
		unsigned int mclkTicksForNextPixelClockTick;
		mclkTicksForNextPixelClockTick = GetMclkTicksForOnePixelClockTick(*hscanSettings, _renderDigitalHCounterPos, _renderDigitalScreenModeRS0Active, _renderDigitalScreenModeRS1Active);

		// Advance a complete pixel clock step if we are able to complete it in this update
		// step, otherwise store the remaining mclk cycles, and terminate the loop.
		if (mclkCyclesRemainingToAdvance >= mclkTicksForNextPixelClockTick)
		{
			// Perform any digital render operations which need to occur on this cycle
			UpdateDigitalRenderProcess(accessTarget, *hscanSettings, *vscanSettings);

			// Perform any analog render operations which need to occur on this cycle
			UpdateAnalogRenderProcess(accessTarget, *hscanSettings, *vscanSettings);

			// If we're about to increment the vcounter, save the current value of it
			// before the increment, so that the analog render process can use it to
			// calculate the current analog output line.
			if ((_renderDigitalHCounterPos + 1) == hscanSettings->vcounterIncrementPoint)
			{
				_renderDigitalVCounterPosPreviousLine = _renderDigitalVCounterPos;
			}

			// Advance the HV counters for the digital render process
			AdvanceHVCountersOneStep(*hscanSettings, _renderDigitalHCounterPos, *vscanSettings, _renderDigitalInterlaceEnabledActive, _renderDigitalOddFlagSet, _renderDigitalVCounterPos);

			// Advance the mclk cycle progress of the current render timeslice
			mclkCyclesRemainingToAdvance -= mclkTicksForNextPixelClockTick;
			_renderDigitalMclkCycleProgress += mclkTicksForNextPixelClockTick;
			_renderDigitalRemainingMclkCycles = mclkCyclesRemainingToAdvance;
		}
		else
		{
			// Save any remaining mclk cycles from this update step
			_renderDigitalRemainingMclkCycles = mclkCyclesRemainingToAdvance;

			// Clear the count of mclk cycles remaining to advance now that we've reached a
			// step that we can't complete.
			mclkCyclesRemainingToAdvance = 0;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::UpdateDigitalRenderProcess(const AccessTarget& accessTarget, const HScanSettings& hscanSettings, const VScanSettings& vscanSettings)
{
	// Use the current VCounter data to determine whether we are rendering an active line
	// of the display, and which active line number we're up to, based on the current
	// screen mode settings.
	bool insideActiveScanRow = false;
	int renderDigitalCurrentRow = -1;
	if ((_renderDigitalVCounterPos >= vscanSettings.activeDisplayVCounterFirstValue) && (_renderDigitalVCounterPos <= vscanSettings.activeDisplayVCounterLastValue))
	{
		// We're inside the active display region. Calculate the active display row number
		// for this row.
		insideActiveScanRow = true;
		renderDigitalCurrentRow = _renderDigitalVCounterPos - vscanSettings.activeDisplayVCounterFirstValue;
	}
	else if (_renderDigitalVCounterPos == vscanSettings.vcounterMaxValue)
	{
		// We're in the first line before the active display. On this line, no actual pixel
		// data is output, but the same access restrictions apply as in an active scan
		// line. Sprite mapping data is also read during this line, to determine which
		// sprites to show on the first line of the display.
		insideActiveScanRow = true;
		renderDigitalCurrentRow = -1;
	}

	// Obtain the linear hcounter value for the current render position
	unsigned int hcounterLinear = HCounterValueFromVDPInternalToLinear(hscanSettings, _renderDigitalHCounterPos);

	// Perform a VSRAM read cache operation if one is required. Note that hardware tests
	// have confirmed this runs even if the display is disabled, or the current line is not
	// within the active display region. Note that hardware tests have also shown that the
	// H32/H40 screen mode setting is not taken into account when reading this data, and
	// that the scroll data for cells only visible in H40 mode is still read in H32 mode
	// when the hcounter reaches the necessary target locations.
	//##TODO## Confirm the exact time this register is latched, using the stable
	// raster DMA method as a testing mechanism.
	//##TODO## Clean up the following old comments
	//##FIX## We've heard reports from Mask of Destiny, and our current corruption in
	// Panorama Cotton seems to confirm, that when the VSCROLL mode is set to overall,
	// the VSRAM data is apparently latched only once for the line, not read at the
	// start of every 2 cell block. Changing this behaviour fixes Panorama Cotton, but
	// it seems to contradict tests done to measure the VSRAM read slots. Actually,
	// this has yet to be confirmed. We need to repeat our VSRAM read tests for which
	// values are read, when the vscroll mode is set to overall. Most likely, the
	// correct VSRAM reads are always done for 2-cell vertical scrolling, but when
	// overall scrolling is enabled, only the read at the start of the line is latched
	// and used to update the cached value. We need to do hardware tests to confirm the
	// correct behaviour.
	//##NOTE## Implementing the caching for vertical scrolling has broken the Sonic 3D
	// blast special stages, but we believe this is caused by bad horizontal interrupt
	// timing rather than this caching itself.
	bool vsramCacheOperationRequired = ((_renderDigitalHCounterPos & 0x007) == 0);
	if (vsramCacheOperationRequired)
	{
		// Calculate the target column and layer for this VSRAM cache operation
		unsigned int vsramColumnNumber = (_renderDigitalHCounterPos >> 4);
		unsigned int vsramLayerNumber = (_renderDigitalHCounterPos & 0x008) >> 3;

		// Determine if interlace mode 2 is currently active
		bool interlaceMode2Active = _renderDigitalInterlaceEnabledActive && _renderDigitalInterlaceDoubleActive;

		// Read registers which affect the read of vscroll data
		bool vscrState = RegGetVSCR(accessTarget);

		// Read vscroll data for the target layer if required. Note that new data is only
		// latched if column vertical scrolling is enabled, or if this is the first column
		// in the display. Panorama Cotton relies on this in the intro screen and during
		// levels.
		if (vscrState || (vsramColumnNumber == 0))
		{
			unsigned int& targetLayerPatternDisplacement = (vsramLayerNumber == 0)? _renderLayerAVscrollPatternDisplacement: _renderLayerBVscrollPatternDisplacement;
			unsigned int& targetLayerMappingDisplacement = (vsramLayerNumber == 0)? _renderLayerAVscrollMappingDisplacement: _renderLayerBVscrollMappingDisplacement;
			DigitalRenderReadVscrollData(vsramColumnNumber, vsramLayerNumber, vscrState, interlaceMode2Active, targetLayerPatternDisplacement, targetLayerMappingDisplacement, _renderVSRAMCachedRead);
		}
	}

	// Obtain the set of internal update steps for the current raster position based on the
	// current screen mode settings.
	unsigned int internalOperationArraySize = 0;
	const InternalRenderOp* internalOperationArray = 0;
	if (_renderDigitalScreenModeRS1Active)
	{
		internalOperationArray = &InternalOperationsH40[0];
		internalOperationArraySize = sizeof(InternalOperationsH40) / sizeof(InternalOperationsH40[0]);
	}
	else
	{
		internalOperationArray = &InternalOperationsH32[0];
		internalOperationArraySize = sizeof(InternalOperationsH32) / sizeof(InternalOperationsH32[0]);
	}

	// Perform the next internal update step for the current hcounter location
	DebugAssert(hcounterLinear < internalOperationArraySize);
	const InternalRenderOp& nextInternalOperation = internalOperationArray[hcounterLinear];
	PerformInternalRenderOperation(accessTarget, hscanSettings, vscanSettings, nextInternalOperation, renderDigitalCurrentRow);

	// Read the display enable register. If this register is cleared, the output for this
	// update step is forced to the background colour, and free access to VRAM is
	// permitted. Any read operations that would have been performed from VRAM are not
	// performed in this case. If this occurs during the active region of the display, it
	// corrupts the output, due to mapping and pattern data not being read at the
	// appropriate time. It is safe to disable the display during the portion of a scanline
	// that lies outside the active display area, however, this reduces the number of
	// sprite pattern blocks which can be read, creating a lower sprite limit on the
	// following scanline. A sprite overflow is flagged if not all the sprite pixels could
	// be read due to the display being disabled, even if there would normally be enough
	// time to read all the pattern data had the display not been disabled. The only game
	// which is known to rely on this is "Mickey Mania" for the Mega Drive. See
	// http://gendev.spritesmind.net/forum/viewtopic.php?t=541
	//##TODO## Determine if the display enable bit is effective when the VDP test
	// register has been set to one of the modes that disables the blanking of the
	// display in the border regions.
	bool displayEnabled = RegGetDisplayEnabled(accessTarget);

	// Obtain the set of VRAM update steps for the current raster position based on the
	// current screen mode settings. If this line is outside the active display area, IE,
	// in the top or bottom border areas or in vblank, or if the display is currently
	// disabled, free access is permitted to VRAM except during the memory refresh slots,
	// so a different set of update steps apply. Note that there is an additional refresh
	// slot in non-active lines compared to active lines.
	unsigned int vramOperationArraySize = 0;
	const VRAMRenderOp* vramOperationArray = 0;
	if (!displayEnabled || !insideActiveScanRow)
	{
		if (_renderDigitalScreenModeRS1Active)
		{
			vramOperationArray = &VramOperationsH40InactiveLine[0];
			vramOperationArraySize = sizeof(VramOperationsH40InactiveLine) / sizeof(VramOperationsH40InactiveLine[0]);
		}
		else
		{
			vramOperationArray = &VramOperationsH32InactiveLine[0];
			vramOperationArraySize = sizeof(VramOperationsH32InactiveLine) / sizeof(VramOperationsH32InactiveLine[0]);
		}
	}
	else
	{
		if (_renderDigitalScreenModeRS1Active)
		{
			vramOperationArray = &VramOperationsH40ActiveLine[0];
			vramOperationArraySize = sizeof(VramOperationsH40ActiveLine) / sizeof(VramOperationsH40ActiveLine[0]);
		}
		else
		{
			vramOperationArray = &VramOperationsH32ActiveLine[0];
			vramOperationArraySize = sizeof(VramOperationsH32ActiveLine) / sizeof(VramOperationsH32ActiveLine[0]);
		}
	}

	// Perform any VRAM render operations which need to occur on this cycle. Note that VRAM
	// render operations only occur once every 4 SC cycles, since it takes the VRAM 4 SC
	// cycles for each 32-bit serial memory read, which is performed by the VDP itself to
	// read VRAM data for the rendering process, or 4 SC cycles for an 8-bit direct memory
	// read or write, which can occur at an access slot. Every 2 SC cycles however, a pixel
	// is output to the analog output circuit to perform layer priority selection and video
	// output. Interestingly, the synchronization of the memory times with the hcounter
	// update process is different, depending on whether H40 mode is active. Where a H32
	// mode is selected, memory access occurs on odd hcounter values. Where H40 mode is
	// selected, memory access occurs on even hcounter values.
	//##TODO## Perform more hardware tests on this behaviour, to confirm the
	// synchronization differences, and determine whether it is the memory access timing or
	// the hcounter progression which changes at the time the H40 screen mode setting is
	// toggled.
	bool hcounterLowerBit = (_renderDigitalHCounterPos & 0x1) != 0;
	if (_renderDigitalScreenModeRS1Active != hcounterLowerBit)
	{
		// Determine what VRAM operation to perform at the current scanline position
		DebugAssert((hcounterLinear >> 1) < vramOperationArraySize);
		const VRAMRenderOp& nextVRAMOperation = vramOperationArray[(hcounterLinear >> 1)];

		// Perform the VRAM operation
		PerformVRAMRenderOperation(accessTarget, hscanSettings, vscanSettings, nextVRAMOperation, renderDigitalCurrentRow);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::PerformInternalRenderOperation(const AccessTarget& accessTarget, const HScanSettings& hscanSettings, const VScanSettings& vscanSettings, const InternalRenderOp& nextOperation, int renderDigitalCurrentRow)
{
	switch (nextOperation.operation)
	{
	case InternalRenderOp::SPRITEMAPPINGCLEAR:
		// If we're about to start reading sprite mapping data for the next line, we need
		// to reset some internal state here.
		//##TODO## Ensure that on a reset, this data is initialized correctly
		//##TODO## Test if the first line of the display always acts like a dot
		// overflow didn't occur on the previous line, or if the VDP remembers if a dot
		// overflow occurred on the very last line of the display when loading sprites
		// for the first line of the display.
		_renderSpriteDotOverflowPreviousLine = _renderSpriteDotOverflow;
		_renderSpriteDotOverflow = false;
		_renderSpriteDisplayCellCacheEntryCount = 0;
		_nonSpriteMaskCellEncountered = false;
		_renderSpriteMaskActive = false;
		_renderSpriteCollision = false;

		// Advance the sprite pixel buffer plane
		_renderSpritePixelBufferAnalogRenderPlane = (_renderSpritePixelBufferAnalogRenderPlane + 1) % renderSpritePixelBufferPlaneCount;
		_renderSpritePixelBufferDigitalRenderPlane = (_renderSpritePixelBufferDigitalRenderPlane + 1) % renderSpritePixelBufferPlaneCount;
		break;
	case InternalRenderOp::SPRITEPATTERNCLEAR:
		// If we're about to start reading sprite pattern data for the next line, reset any
		// necessary state so that we can correctly start working with the new data.
		//##TODO## Ensure that on a reset, this data is initialized correctly
		_renderSpriteSearchComplete = false;
		_renderSpriteOverflow = false;
		_renderSpriteNextAttributeTableEntryToRead = 0;
		_renderSpriteDisplayCacheEntryCount = 0;
		_renderSpriteDisplayCellCacheCurrentIndex = 0;
		_renderSpriteDisplayCacheCurrentIndex = 0;

		// Clear the contents of the sprite pixel buffer for this line. Note that most
		// likely in the real VDP, the analog render process would do this for us as it
		// pulls sprite data out of the buffer. We don't want to rely on the render process
		// running at all in our core though, so we do it as a manual step here.
		for (unsigned int i = 0; i < spritePixelBufferSize; ++i)
		{
			_spritePixelBuffer[_renderSpritePixelBufferDigitalRenderPlane][i].entryWritten = false;
		}
		break;
	case InternalRenderOp::SPRITECACHE:{
		// Calculate the row number of the next line of sprite data to fetch. We add a
		// value of 1 to the current row number because sprites are first parsed one line
		// before the line on which they are displayed, since there are three separate
		// rendering phases for sprites, the first of which is a full traversal of the
		// cached sprite attribute table data, which occurs on the row before the row at
		// which we're searching for sprites to render for. Note that we rely on the
		// current row number being set to -1 for the invisible line before the first
		// display line in the active display region here.
		unsigned int renderSpriteNextSpriteRow = renderDigitalCurrentRow + 1;

		// Determine if interlace mode 2 is currently active
		bool interlaceMode2Active = _renderDigitalInterlaceEnabledActive && _renderDigitalInterlaceDoubleActive;

		// Read the next entry out of the sprite cache
		DigitalRenderBuildSpriteList(renderSpriteNextSpriteRow, interlaceMode2Active, _renderDigitalScreenModeRS1Active, _renderSpriteNextAttributeTableEntryToRead, _renderSpriteSearchComplete, _renderSpriteOverflow, _renderSpriteDisplayCacheEntryCount, _renderSpriteDisplayCache);
		break;}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::PerformVRAMRenderOperation(const AccessTarget& accessTarget, const HScanSettings& hscanSettings, const VScanSettings& vscanSettings, const VRAMRenderOp& nextOperation, int renderDigitalCurrentRow)
{
	// Determine if interlace mode 2 is currently active
	bool interlaceMode2Active = _renderDigitalInterlaceEnabledActive && _renderDigitalInterlaceDoubleActive;

	// Perform the next operation
	switch (nextOperation.operation)
	{
	case VRAMRenderOp::HSCROLL:{
		// Skip the operation if this is the invisible line just before the display region
		if (renderDigitalCurrentRow < 0) break;

		// Read the current mode register settings
		bool extendedVRAMModeActive = RegGetEVRAM(accessTarget);

		// Read register settings which affect which hscroll data is loaded
		unsigned int hscrollDataBase = RegGetHScrollDataBase(accessTarget, extendedVRAMModeActive);
		bool hscrState = RegGetHSCR(accessTarget);
		bool lscrState = RegGetLSCR(accessTarget);

		// Load the hscroll data into the hscroll data cache
		DigitalRenderReadHscrollData(renderDigitalCurrentRow, hscrollDataBase, hscrState, lscrState, _renderLayerAHscrollPatternDisplacement, _renderLayerBHscrollPatternDisplacement, _renderLayerAHscrollMappingDisplacement, _renderLayerBHscrollMappingDisplacement);
		break;}
	case VRAMRenderOp::MAPPING_A:{
		// Skip the operation if this is the invisible line just before the display region
		if (renderDigitalCurrentRow < 0) break;

		// Calculate the index number of the current 2-cell column that's being rendered.
		// Note that we subtract 1 from the index number specified in the operation table.
		// The first mapping data we read is for the left-scrolled 2-cell column, which
		// reads its mapping data from cell block -1.
		unsigned int renderDigitalCurrentColumn = (nextOperation.index - 1) / CellsPerColumn;

		// Read the current mode register settings
		bool mode4Active = !RegGetMode5(accessTarget);
		bool extendedVRAMModeActive = RegGetEVRAM(accessTarget);

		// Read the current window register settings
		bool windowAlignedRight = RegGetWindowRightAligned(accessTarget);
		bool windowAlignedBottom = RegGetWindowBottomAligned(accessTarget);
		unsigned int windowBasePointX = RegGetWindowBasePointX(accessTarget);
		unsigned int windowBasePointY = RegGetWindowBasePointY(accessTarget);

		// Calculate the screen position of the window layer
		unsigned int windowStartCellX = (windowAlignedRight)? windowBasePointX: 0;
		unsigned int windowEndCellX = (windowAlignedRight)? 9999: windowBasePointX;
		unsigned int windowStartCellY = (windowAlignedBottom)? windowBasePointY: 0;
		unsigned int windowEndCellY = (windowAlignedBottom)? 9999: windowBasePointY;

		// Determine if the window is active in the current column and row
		//##TODO## Perform hardware tests on changing the window register state mid-line.
		// We need to determine how the VDP responds, in particular, how the window
		// distortion bug behaves when the window is enabled or disabled mid-line.
		static const unsigned int rowsToDisplayPerCell = 8;
		unsigned int currentCellRow = renderDigitalCurrentRow / rowsToDisplayPerCell;
		bool windowActiveInThisColumn = false;
		if (nextOperation.index > 0)
		{
			windowActiveInThisColumn = ((renderDigitalCurrentColumn >= windowStartCellX) && (renderDigitalCurrentColumn < windowEndCellX))
			                        || ((currentCellRow >= windowStartCellY) && (currentCellRow < windowEndCellY));
			_renderWindowActiveCache[renderDigitalCurrentColumn] = windowActiveInThisColumn;
		}

		// Calculate the effective screen row number to use when calculating the mapping
		// data row for this line. Note that the screen row number is just used directly
		// under normal circumstances, but when interlace mode 2 is active, there are
		// effectively double the number of vertical screen rows, with all the odd rows in
		// one frame, and all the even rows in another frame, so if interlace mode 2 is
		// active we double the screen row number and select either odd or even rows based
		// on the current state of the odd flag.
		unsigned int screenRowIndex = renderDigitalCurrentRow;
		if (interlaceMode2Active)
		{
			screenRowIndex *= 2;
			if (_renderDigitalOddFlagSet)
			{
				++screenRowIndex;
			}
		}

		// Read the correct mapping data for this column, taking into account whether that
		// mapping data should be read from the window mapping table or the layer A mapping
		// table.
		if (windowActiveInThisColumn)
		{
			// Read register settings which affect which mapping data is loaded
			unsigned int nameTableBase = RegGetNameTableBaseWindow(accessTarget, _renderDigitalScreenModeRS1Active, extendedVRAMModeActive);

			// The window mapping table dimensions are determined based on the H40 screen
			// mode state. If H40 mode is active, the window mapping table is 64 cells
			// wide, otherwise, it is 32 cells wide. We emulate that here by calculating
			// the mapping table dimensions ourselves and passing this data in when reading
			// the window layer mapping data.
			unsigned int hszState = (_renderDigitalScreenModeRS1Active)? 0x1: 0x0;
			unsigned int vszState = 0x0;

			// Read window mapping data
			unsigned int mappingDataAddressWindow = DigitalRenderCalculateMappingVRAMAddess(screenRowIndex, renderDigitalCurrentColumn, interlaceMode2Active, nameTableBase, 0, 0, 0, hszState, vszState);
			_renderMappingDataCacheLayerA[nextOperation.index] = ((unsigned int)_vram->ReadCommitted(mappingDataAddressWindow+0) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddressWindow+1);
			_renderMappingDataCacheLayerA[nextOperation.index+1] = ((unsigned int)_vram->ReadCommitted(mappingDataAddressWindow+2) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddressWindow+3);
			_renderMappingDataCacheSourceAddressLayerA[nextOperation.index] = mappingDataAddressWindow;
			_renderMappingDataCacheSourceAddressLayerA[nextOperation.index+1] = mappingDataAddressWindow+2;

			// Apply layer removal for the window layer
			if (!_enableWindowHigh || !_enableWindowLow)
			{
				// Read register settings which affect which mapping data is loaded
				nameTableBase = RegGetNameTableBaseScrollA(accessTarget, mode4Active, extendedVRAMModeActive);
				hszState = RegGetHSZ(accessTarget);
				vszState = RegGetVSZ(accessTarget);

				// Read layer A mapping data
				Data layerAMapping1(16);
				Data layerAMapping2(16);
				unsigned int mappingDataAddressLayerA = DigitalRenderCalculateMappingVRAMAddess(screenRowIndex, renderDigitalCurrentColumn, interlaceMode2Active, nameTableBase, _renderLayerAHscrollMappingDisplacement, _renderLayerAVscrollMappingDisplacement, _renderLayerAVscrollPatternDisplacement, hszState, vszState);
				layerAMapping1 = ((unsigned int)_vram->ReadCommitted(mappingDataAddressLayerA+0) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddressLayerA+1);
				layerAMapping2 = ((unsigned int)_vram->ReadCommitted(mappingDataAddressLayerA+2) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddressLayerA+3);

				// Substitute layer A mapping data for any mapping blocks in the window
				// region that are hidden by layer removal
				bool windowMapping1Priority = _renderMappingDataCacheLayerA[nextOperation.index].MSB();
				bool windowMapping2Priority = _renderMappingDataCacheLayerA[nextOperation.index+1].MSB();
				if ((windowMapping1Priority && !_enableWindowHigh) || (!windowMapping1Priority && !_enableWindowLow)
				|| (windowMapping2Priority && !_enableWindowHigh) || (!windowMapping2Priority && !_enableWindowLow))
				{
					_renderWindowActiveCache[renderDigitalCurrentColumn] = false;
					_renderMappingDataCacheLayerA[nextOperation.index] = layerAMapping1;
					_renderMappingDataCacheLayerA[nextOperation.index+1] = layerAMapping2;
					_renderMappingDataCacheSourceAddressLayerA[nextOperation.index] = mappingDataAddressLayerA;
					_renderMappingDataCacheSourceAddressLayerA[nextOperation.index+1] = mappingDataAddressLayerA+2;
				}
			}
		}
		else
		{
			// Read register settings which affect which mapping data is loaded
			unsigned int nameTableBase = RegGetNameTableBaseScrollA(accessTarget, mode4Active, extendedVRAMModeActive);
			unsigned int hszState = RegGetHSZ(accessTarget);
			unsigned int vszState = RegGetVSZ(accessTarget);

			// Read layer A mapping data
			unsigned int mappingDataAddress = DigitalRenderCalculateMappingVRAMAddess(screenRowIndex, renderDigitalCurrentColumn, interlaceMode2Active, nameTableBase, _renderLayerAHscrollMappingDisplacement, _renderLayerAVscrollMappingDisplacement, _renderLayerAVscrollPatternDisplacement, hszState, vszState);
			_renderMappingDataCacheLayerA[nextOperation.index] = ((unsigned int)_vram->ReadCommitted(mappingDataAddress+0) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddress+1);
			_renderMappingDataCacheLayerA[nextOperation.index+1] = ((unsigned int)_vram->ReadCommitted(mappingDataAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddress+3);
			_renderMappingDataCacheSourceAddressLayerA[nextOperation.index] = mappingDataAddress;
			_renderMappingDataCacheSourceAddressLayerA[nextOperation.index+1] = mappingDataAddress+2;
		}

		break;}
	case VRAMRenderOp::PATTERN_A:{
		// Skip the operation if this is the invisible line just before the display region
		if (renderDigitalCurrentRow < 0) break;

		// Calculate the vertical pattern displacement value to use to calculate the row of
		// the pattern data to read. The only purpose of this code is to handle window
		// layer support. In regions where the window layer is active, we need to disable
		// vertical scrolling, so if this cell number is within the window region of the
		// display, we force the vertical pattern displacement to 0, otherwise, we use the
		// calculated vertical pattern displacement based on the layer A vscroll data.
		unsigned int verticalPatternDisplacement = _renderLayerAVscrollPatternDisplacement;
		if (nextOperation.index >= 2)
		{
			unsigned int renderDigitalCurrentColumn = (nextOperation.index - 2) / CellsPerColumn;
			if (_renderWindowActiveCache[renderDigitalCurrentColumn])
			{
				verticalPatternDisplacement = (interlaceMode2Active && _renderDigitalOddFlagSet)? 1: 0;
			}
		}

		// Calculate the pattern row number to read for the selected pattern, based on the
		// current row being drawn on the screen, and the vertical pattern displacement due
		// to vscroll. We also factor in interlace mode 2 support here by doubling the
		// screen row number where interlace mode 2 is active. The state of the odd flag is
		// factored in when the vscroll data is read.
		static const unsigned int rowsToDisplayPerTile = 8;
		unsigned int screenPatternRowIndex = renderDigitalCurrentRow % rowsToDisplayPerTile;
		if (interlaceMode2Active)
		{
			screenPatternRowIndex *= 2;
			if (_renderDigitalOddFlagSet)
			{
				++screenPatternRowIndex;
			}
		}
		const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
		unsigned int patternRowNumberNoFlip = (screenPatternRowIndex + verticalPatternDisplacement) % rowsPerTile;

		// Calculate the address of the target pattern data row
		const Data& mappingData = _renderMappingDataCacheLayerA[nextOperation.index];
		unsigned int patternRowNumber = CalculatePatternDataRowNumber(patternRowNumberNoFlip, interlaceMode2Active, mappingData);
		unsigned int patternDataAddress = CalculatePatternDataRowAddress(patternRowNumber, 0, interlaceMode2Active, mappingData);

		// Read the target pattern row
		_renderPatternDataCacheLayerA[nextOperation.index] = ((unsigned int)_vram->ReadCommitted(patternDataAddress+0) << 24) | ((unsigned int)_vram->ReadCommitted(patternDataAddress+1) << 16) | ((unsigned int)_vram->ReadCommitted(patternDataAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(patternDataAddress+3);
		_renderPatternDataCacheRowNoLayerA[nextOperation.index] = patternRowNumber;
		break;}
	case VRAMRenderOp::MAPPING_B:{
		// Skip the operation if this is the invisible line just before the display region
		if (renderDigitalCurrentRow < 0) break;

		// Calculate the index number of the current 2-cell column that's being rendered.
		// Note that we subtract 1 from the index number specified in the operation table.
		// The first mapping data we read is for the left-scrolled 2-cell column, which
		// reads its mapping data from cell block -1.
		unsigned int renderDigitalCurrentColumn = (nextOperation.index - 1) / CellsPerColumn;

		// Read the current mode register settings
		bool extendedVRAMModeActive = RegGetEVRAM(accessTarget);

		// Calculate the effective screen row number to use when calculating the mapping
		// data row for this line. Note that the screen row number is just used directly
		// under normal circumstances, but when interlace mode 2 is active, there are
		// effectively double the number of vertical screen rows, with all the odd rows in
		// one frame, and all the even rows in another frame, so if interlace mode 2 is
		// active we double the screen row number and select either odd or even rows based
		// on the current state of the odd flag.
		unsigned int screenRowIndex = renderDigitalCurrentRow;
		if (interlaceMode2Active)
		{
			screenRowIndex *= 2;
			if (_renderDigitalOddFlagSet)
			{
				++screenRowIndex;
			}
		}

		// Read register settings which affect which mapping data is loaded
		unsigned int nameTableBase = RegGetNameTableBaseScrollB(accessTarget, extendedVRAMModeActive);
		unsigned int hszState = RegGetHSZ(accessTarget);
		unsigned int vszState = RegGetVSZ(accessTarget);

		// Read layer B mapping data
		unsigned int mappingDataAddress = DigitalRenderCalculateMappingVRAMAddess(screenRowIndex, renderDigitalCurrentColumn, interlaceMode2Active, nameTableBase, _renderLayerBHscrollMappingDisplacement, _renderLayerBVscrollMappingDisplacement, _renderLayerBVscrollPatternDisplacement, hszState, vszState);
		_renderMappingDataCacheLayerB[nextOperation.index] = ((unsigned int)_vram->ReadCommitted(mappingDataAddress+0) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddress+1);
		_renderMappingDataCacheLayerB[nextOperation.index+1] = ((unsigned int)_vram->ReadCommitted(mappingDataAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(mappingDataAddress+3);
		_renderMappingDataCacheSourceAddressLayerB[nextOperation.index] = mappingDataAddress;
		_renderMappingDataCacheSourceAddressLayerB[nextOperation.index+1] = mappingDataAddress+2;
		break;}
	case VRAMRenderOp::PATTERN_B:{
		// Skip the operation if this is the invisible line just before the display region
		if (renderDigitalCurrentRow < 0) break;

		// Calculate the pattern row number to read for the selected pattern, based on the
		// current row being drawn on the screen, and the vertical pattern displacement due
		// to vscroll. We also factor in interlace mode 2 support here by doubling the
		// screen row number where interlace mode 2 is active. The state of the odd flag is
		// factored in when the vscroll data is read.
		static const unsigned int rowsToDisplayPerTile = 8;
		unsigned int screenPatternRowIndex = renderDigitalCurrentRow % rowsToDisplayPerTile;
		if (interlaceMode2Active)
		{
			screenPatternRowIndex *= 2;
			if (_renderDigitalOddFlagSet)
			{
				++screenPatternRowIndex;
			}
		}
		const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
		unsigned int patternRowNumberNoFlip = (screenPatternRowIndex + _renderLayerBVscrollPatternDisplacement) % rowsPerTile;

		// Calculate the address of the target pattern data row
		const Data& mappingData = _renderMappingDataCacheLayerB[nextOperation.index];
		unsigned int patternRowNumber = CalculatePatternDataRowNumber(patternRowNumberNoFlip, interlaceMode2Active, mappingData);
		unsigned int patternDataAddress = CalculatePatternDataRowAddress(patternRowNumber, 0, interlaceMode2Active, mappingData);

		// Read the target pattern row
		_renderPatternDataCacheLayerB[nextOperation.index] = ((unsigned int)_vram->ReadCommitted(patternDataAddress+0) << 24) | ((unsigned int)_vram->ReadCommitted(patternDataAddress+1) << 16) | ((unsigned int)_vram->ReadCommitted(patternDataAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(patternDataAddress+3);
		_renderPatternDataCacheRowNoLayerB[nextOperation.index] = patternRowNumber;
		break;}
	case VRAMRenderOp::MAPPING_S:{
		// Read the current mode register settings
		bool mode4Active = !RegGetMode5(accessTarget);
		bool extendedVRAMModeActive = RegGetEVRAM(accessTarget);

		// Read sprite mapping data and decode sprite cells for the next sprite to display
		// on the current scanline.
		if (_renderSpriteDisplayCacheCurrentIndex < _renderSpriteDisplayCacheEntryCount)
		{
			// Read the sprite table base address register
			unsigned int spriteTableBaseAddress = RegGetNameTableBaseSprite(accessTarget, mode4Active, _renderDigitalScreenModeRS1Active, extendedVRAMModeActive);

			// Obtain a reference to the corresponding sprite display cache entry for this
			// sprite.
			SpriteDisplayCacheEntry& spriteDisplayCacheEntry = _renderSpriteDisplayCache[_renderSpriteDisplayCacheCurrentIndex];

			// Build a sprite cell list for the current sprite
			DigitalRenderBuildSpriteCellList(hscanSettings, vscanSettings, _renderSpriteDisplayCacheCurrentIndex, spriteTableBaseAddress, interlaceMode2Active, _renderDigitalScreenModeRS1Active, _renderSpriteDotOverflow, spriteDisplayCacheEntry, _renderSpriteDisplayCellCacheEntryCount, _renderSpriteDisplayCellCache);

			// Advance to the next sprite mapping entry
			++_renderSpriteDisplayCacheCurrentIndex;
		}
		break;}
	case VRAMRenderOp::PATTERN_S:{
		// Read sprite pattern data for the next sprite cell to display on the current
		// scanline. Note that we know that if the display is disabled during active scan,
		// sprite pattern data which was supposed to be read during the disabled region
		// isn't skipped, but rather, the entire sprite pattern data load queue remains in
		// its current state, and the next sprite pattern data block to read is retrieved
		// from the queue when a slot becomes available. This means we can't use a simple
		// index number to determine which sprite data to read, but rather, we need to keep
		// a running index of the sprite pattern data we're up to.
		//##TODO## Check if the sprite collision flag is still set for sprite pixels that
		// are masked.
		if (_renderSpriteDisplayCellCacheCurrentIndex < _renderSpriteDisplayCellCacheEntryCount)
		{
			// Obtain a reference to the corresponding sprite cell and sprite display cache
			// entries for this sprite cell. Note that only the mappingData and hpos members
			// of the sprite display cache entry are valid at this point, as other members
			// of the structure will already have been updated for the following line.
			SpriteCellDisplayCacheEntry& spriteCellDisplayCacheEntry = _renderSpriteDisplayCellCache[_renderSpriteDisplayCellCacheCurrentIndex];
			SpriteDisplayCacheEntry& spriteDisplayCacheEntry = _renderSpriteDisplayCache[spriteCellDisplayCacheEntry.spriteDisplayCacheIndex];

			// Calculate the address of the target pattern data row for the sprite cell
			unsigned int patternCellOffset = (spriteCellDisplayCacheEntry.patternCellOffsetX * spriteCellDisplayCacheEntry.spriteHeightInCells) + spriteCellDisplayCacheEntry.patternCellOffsetY;
			unsigned int patternRowNumber = CalculatePatternDataRowNumber(spriteCellDisplayCacheEntry.patternRowOffset, interlaceMode2Active, spriteDisplayCacheEntry.mappingData);
			unsigned int patternDataAddress = CalculatePatternDataRowAddress(patternRowNumber, patternCellOffset, interlaceMode2Active, spriteDisplayCacheEntry.mappingData);

			// Read the target pattern row
			spriteCellDisplayCacheEntry.patternData = ((unsigned int)_vram->ReadCommitted(patternDataAddress+0) << 24) | ((unsigned int)_vram->ReadCommitted(patternDataAddress+1) << 16) | ((unsigned int)_vram->ReadCommitted(patternDataAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(patternDataAddress+3);

			// Read all used bits of the horizontal position data for this sprite
			Data spritePosH(9);
			spritePosH = spriteDisplayCacheEntry.hpos;

			// Take into account sprite masking
			//##TODO## Add a much longer comment here describing sprite masking
			if (spritePosH == 0)
			{
				if (_nonSpriteMaskCellEncountered || _renderSpriteDotOverflowPreviousLine)
				{
					_renderSpriteMaskActive = true;
				}
			}
			else
			{
				// Since we've now encountered at least one sprite pattern cell which was
				// not a mask sprite cell, set a flag indicating that that we've
				// encountered a non mask cell. This is required in order to correctly
				// support sprite masking.
				_nonSpriteMaskCellEncountered = true;
			}

			// If this sprite cell was not masked, load visible pixel information into the
			// sprite pixel buffer.
			//##TODO## Greatly improve commenting here.
			if (!_renderSpriteMaskActive)
			{
				static const unsigned int spritePosScreenStartH = 0x80;
				static const unsigned int cellPixelWidth = 8;
				for (unsigned int cellPixelIndex = 0; cellPixelIndex < cellPixelWidth; ++cellPixelIndex)
				{
					// Check that this sprite pixel is within the visible screen boundaries
					unsigned int spritePixelH = spritePosH.GetData() + (spriteCellDisplayCacheEntry.spriteCellColumnNo * cellPixelWidth) + cellPixelIndex;
					if ((spritePixelH >= spritePosScreenStartH) && ((spritePixelH - spritePosScreenStartH) < spritePixelBufferSize))
					{
						// Check that a non-transparent sprite pixel has not already been
						// written to this pixel location. Higher priority sprites are
						// listed first, with lower priority sprites appearing under higher
						// priority sprites, so if a non-transparent sprite pixel has
						// already been output to this location, the lower priority sprite
						// pixel is discarded, and the sprite collision flag is set in the
						// status register.
						unsigned int spritePixelBufferIndex = (spritePixelH - spritePosScreenStartH);
						SpritePixelBufferEntry& spritePixelBufferEntry = _spritePixelBuffer[_renderSpritePixelBufferDigitalRenderPlane][spritePixelBufferIndex];
						if (!spritePixelBufferEntry.entryWritten || (spritePixelBufferEntry.paletteIndex == 0))
						{
							// Mapping (Pattern Name) data format:
							// -----------------------------------------------------------------
							// |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
							// |---------------------------------------------------------------|
							// |Pri|PalRow |VF |HF |              Pattern Number               |
							// -----------------------------------------------------------------
							// Pri:    Priority Bit
							// PalRow: The palette row number to use when displaying the pattern data
							// VF:     Vertical Flip
							// HF:     Horizontal Flip
							spritePixelBufferEntry.entryWritten = true;
							spritePixelBufferEntry.layerPriority = spriteDisplayCacheEntry.mappingData.GetBit(15);
							spritePixelBufferEntry.paletteLine = spriteDisplayCacheEntry.mappingData.GetDataSegment(13, 2);
							spritePixelBufferEntry.paletteIndex = DigitalRenderReadPixelIndex(spriteCellDisplayCacheEntry.patternData, spriteDisplayCacheEntry.mappingData.GetBit(11), cellPixelIndex);

							// Record debug info on this sprite pixel buffer entry, so that
							// we can reverse the render pipeline later for debug purposes
							// and determine the sprite that generated this pixel.
							if (_videoEnableFullImageBufferInfo)
							{
								spritePixelBufferEntry.patternRowNo = patternRowNumber;
								spritePixelBufferEntry.patternColumnNo = cellPixelIndex;
								spritePixelBufferEntry.spriteTableEntryNo = spriteCellDisplayCacheEntry.spriteTableIndex;
								spritePixelBufferEntry.spriteTableEntryAddress = spriteCellDisplayCacheEntry.spriteTableEntryAddress;
								spritePixelBufferEntry.spriteMappingData = spriteDisplayCacheEntry.mappingData;
								spritePixelBufferEntry.spriteCellWidth = spriteCellDisplayCacheEntry.spriteWidthInCells;
								spritePixelBufferEntry.spriteCellHeight = spriteCellDisplayCacheEntry.spriteHeightInCells;
								spritePixelBufferEntry.spriteCellPosX = spriteCellDisplayCacheEntry.patternCellOffsetX;
								spritePixelBufferEntry.spriteCellPosY = spriteCellDisplayCacheEntry.patternCellOffsetY;
							}
						}
						else
						{
							_renderSpriteCollision = true;
						}
					}
				}
			}

			// Advance to the next sprite cell entry
			++_renderSpriteDisplayCellCacheCurrentIndex;
		}
		break;}
	case VRAMRenderOp::ACC_SLOT:
		// Since we've reached an external access slot, changes may now be made to VRAM or
		// VSRAM, so we need to advance the VRAM and VSRAM buffers up to this time so any
		// changes which occur at this access slot can take effect.
		_vram->AdvanceBySession(_renderDigitalMclkCycleProgress, _vramSession, _vramTimesliceCopy);
		_vsram->AdvanceBySession(_renderDigitalMclkCycleProgress, _vsramSession, _vsramTimesliceCopy);
		_spriteCache->AdvanceBySession(_renderDigitalMclkCycleProgress, _spriteCacheSession, _spriteCacheTimesliceCopy);
		break;
	case VRAMRenderOp::REFRESH:
		// Nothing to do on a memory refresh cycle
		break;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::UpdateAnalogRenderProcess(const AccessTarget& accessTarget, const HScanSettings& hscanSettings, const VScanSettings& vscanSettings)
{
	bool outputNothing = false;
	bool forceOutputBackgroundPixel = false;

	// If the digital vcounter has already been incremented, but we haven't reached the end
	// of the analog line yet, move the digital vcounter back one step so we can calculate
	// the analog line number.
	unsigned int renderDigitalVCounterPosIncrementAtHBlank = _renderDigitalVCounterPos;
	if ((_renderDigitalHCounterPos >= hscanSettings.vcounterIncrementPoint) && (_renderDigitalHCounterPos < hscanSettings.hblankSetPoint))
	{
		renderDigitalVCounterPosIncrementAtHBlank = _renderDigitalVCounterPosPreviousLine;
	}

	// Use the current VCounter data to determine which data is being displayed on this
	// row, based on the current screen mode settings.
	bool insidePixelBufferRegion = true;
	bool insideActiveScanVertically = false;
	unsigned int renderAnalogCurrentRow = 0;
	if ((renderDigitalVCounterPosIncrementAtHBlank >= vscanSettings.activeDisplayVCounterFirstValue) && (renderDigitalVCounterPosIncrementAtHBlank <= vscanSettings.activeDisplayVCounterLastValue))
	{
		// We're inside the active display region
		renderAnalogCurrentRow = vscanSettings.topBorderLineCount + (renderDigitalVCounterPosIncrementAtHBlank - vscanSettings.activeDisplayVCounterFirstValue);
		insideActiveScanVertically = true;
	}
	else
	{
		// Check if we're in a border region, or in the blanking region.
		if ((renderDigitalVCounterPosIncrementAtHBlank >= vscanSettings.topBorderVCounterFirstValue) && (renderDigitalVCounterPosIncrementAtHBlank <= vscanSettings.topBorderVCounterLastValue))
		{
			// We're in the top border. In this case, we need to force the pixel output to
			// the current backdrop colour.
			renderAnalogCurrentRow = renderDigitalVCounterPosIncrementAtHBlank - vscanSettings.topBorderVCounterFirstValue;
			forceOutputBackgroundPixel = true;
		}
		else if ((renderDigitalVCounterPosIncrementAtHBlank >= vscanSettings.bottomBorderVCounterFirstValue) && (renderDigitalVCounterPosIncrementAtHBlank <= vscanSettings.bottomBorderVCounterLastValue))
		{
			// We're in the bottom border. In this case, we need to force the pixel output
			// to the current backdrop colour.
			renderAnalogCurrentRow = vscanSettings.topBorderLineCount + vscanSettings.activeDisplayLineCount + (renderDigitalVCounterPosIncrementAtHBlank - vscanSettings.bottomBorderVCounterFirstValue);
			forceOutputBackgroundPixel = true;
		}
		else
		{
			// We're in a blanking region. In this case, we need to force the pixel output
			// to black.
			insidePixelBufferRegion = false;
			outputNothing = true;
		}
	}

	// Use the current HCounter data to determine which data is next to be displayed on
	// this line, based on the current screen mode settings.
	bool insideActiveScanHorizontally = false;
	unsigned int renderAnalogCurrentPixel = 0;
	unsigned int activeScanPixelIndex = 0;
	if ((_renderDigitalHCounterPos >= hscanSettings.activeDisplayHCounterFirstValue) && (_renderDigitalHCounterPos <= hscanSettings.activeDisplayHCounterLastValue))
	{
		// We're inside the active display region. Calculate the pixel number of the
		// current pixel to output on this update cycle.
		renderAnalogCurrentPixel = hscanSettings.leftBorderPixelCount + (_renderDigitalHCounterPos - hscanSettings.activeDisplayHCounterFirstValue);
		activeScanPixelIndex = (_renderDigitalHCounterPos - hscanSettings.activeDisplayHCounterFirstValue);
		insideActiveScanHorizontally = true;
	}
	else if ((_renderDigitalHCounterPos >= hscanSettings.leftBorderHCounterFirstValue) && (_renderDigitalHCounterPos <= hscanSettings.leftBorderHCounterLastValue))
	{
		// We're in the left border. In this case, we need to force the pixel output to the
		// current backdrop colour.
		renderAnalogCurrentPixel = (_renderDigitalHCounterPos - hscanSettings.leftBorderHCounterFirstValue);
		forceOutputBackgroundPixel = true;
	}
	else if ((_renderDigitalHCounterPos >= hscanSettings.rightBorderHCounterFirstValue) && (_renderDigitalHCounterPos <= hscanSettings.rightBorderHCounterLastValue))
	{
		// We're in the right border. In this case, we need to force the pixel output to
		// the current backdrop colour.
		renderAnalogCurrentPixel = hscanSettings.leftBorderPixelCount + hscanSettings.activeDisplayPixelCount + (_renderDigitalHCounterPos - hscanSettings.rightBorderHCounterFirstValue);
		forceOutputBackgroundPixel = true;
	}
	else
	{
		// We're in a blanking region or in the hscan region. In this case, there's nothing
		// to output.
		insidePixelBufferRegion = false;
		outputNothing = true;
	}

	// Update the current screen raster position of the render output for debug output
	_currentRenderPosOnScreen = false;
	if (insidePixelBufferRegion)
	{
		_currentRenderPosScreenX = renderAnalogCurrentPixel;
		_currentRenderPosScreenY = renderAnalogCurrentRow;
		_currentRenderPosOnScreen = true;
	}

	// Roll our image buffers on to the next line and the next frame when appropriate
	if (_renderDigitalHCounterPos == hscanSettings.hsyncNegated)
	{
		// Record the number of output pixels we're going to generate in this line
		_imageBufferLineWidth[_drawingImageBufferPlane][renderAnalogCurrentRow] = hscanSettings.leftBorderPixelCount + hscanSettings.activeDisplayPixelCount + hscanSettings.rightBorderPixelCount;

		// Record the active scan start and end positions for this line
		_imageBufferActiveScanPosXStart[_drawingImageBufferPlane][renderAnalogCurrentRow] = hscanSettings.leftBorderPixelCount;
		_imageBufferActiveScanPosXEnd[_drawingImageBufferPlane][renderAnalogCurrentRow] = hscanSettings.leftBorderPixelCount + hscanSettings.activeDisplayPixelCount;
	}
	else if ((_renderDigitalHCounterPos == hscanSettings.vcounterIncrementPoint) && (_renderDigitalVCounterPos == vscanSettings.vsyncClearedPoint))
	{
		// Calculate the image buffer plane to use for the next frame
		unsigned int newDrawingImageBufferPlane = _videoSingleBuffering? _drawingImageBufferPlane: (_drawingImageBufferPlane + 1) % ImageBufferPlanes;

		// Obtain a write lock on the new drawing image buffer plane
		_imageBufferLock[newDrawingImageBufferPlane].ObtainWriteLock();

		// Advance the drawing image buffer to the next plane
		_drawingImageBufferPlane = newDrawingImageBufferPlane;

		// Now that we've completed another frame, advance the last rendered frame token.
		++_lastRenderedFrameToken;

		// Record the odd interlace frame flag
		_imageBufferLineCount[_drawingImageBufferPlane] = _renderDigitalOddFlagSet;

		// Record the number of raster lines we're going to render in the new frame
		_imageBufferLineCount[_drawingImageBufferPlane] = vscanSettings.topBorderLineCount + vscanSettings.activeDisplayLineCount + vscanSettings.bottomBorderLineCount;

		// Record the active scan start and end positions for this frame
		_imageBufferActiveScanPosYStart[_drawingImageBufferPlane] = vscanSettings.topBorderLineCount;
		_imageBufferActiveScanPosYEnd[_drawingImageBufferPlane] = vscanSettings.topBorderLineCount + vscanSettings.activeDisplayLineCount;

		// Clear the cache of sprite boundary lines in this frame
		std::unique_lock<std::mutex> spriteLock(_spriteBoundaryMutex[_drawingImageBufferPlane]);
		_imageBufferSpriteBoundaryLines[_drawingImageBufferPlane].clear();

		// Release the write lock on the image buffer plane
		_imageBufferLock[newDrawingImageBufferPlane].ReleaseWriteLock();
	}

	// Read the display enable register. If this register is cleared, the output for this
	// update step is forced to the background colour, and free access to VRAM is
	// permitted.
	bool displayEnabled = RegGetDisplayEnabled(accessTarget);
	if (!displayEnabled)
	{
		forceOutputBackgroundPixel = true;
	}

	//##TODO## Handle reg 0, bit 0, which completely disables video output while it is
	// set. In our case here, we should force the output colour to black.
	//##TODO## Test on the hardware if we should disable the actual rendering process
	// and allow free access to VRAM if reg 0 bit 0 is set, or if this bit only
	// disables the analog video output.
	//##NOTE## Hardware tests have shown this register only affects the CSYNC output line.
	// Clean up these comments, and ensure we're not doing anything to affect rendering
	// based on this register state.

	// Set the initial data for this pixel info entry
	ImageBufferInfo* imageBufferInfoEntry = 0;
	if (_videoEnableFullImageBufferInfo && insidePixelBufferRegion)
	{
		imageBufferInfoEntry = &_imageBufferInfo[_drawingImageBufferPlane][(renderAnalogCurrentRow * ImageBufferWidth) + renderAnalogCurrentPixel];
		imageBufferInfoEntry->hcounter = _renderDigitalHCounterPos;
		imageBufferInfoEntry->vcounter = _renderDigitalVCounterPos;
		imageBufferInfoEntry->mappingData = 0;
		imageBufferInfoEntry->mappingVRAMAddress = 0;
	}

	// Determine the palette line and index numbers and the shadow/highlight state for this
	// pixel.
	bool shadow = false;
	bool highlight = false;
	unsigned int paletteLine = 0;
	unsigned int paletteIndex = 0;
	if (outputNothing)
	{
		// If a pixel is being forced to black, we currently don't have anything to do
		// here.

		// Record the source layer for this pixel
		if (imageBufferInfoEntry != 0)
		{
			imageBufferInfoEntry->pixelSource = PixelSource::Blanking;
		}
	}
	else if (forceOutputBackgroundPixel)
	{
		// If this pixel is being forced to the background colour, read the current
		// background palette index and line data.
		paletteLine = RegGetBackgroundPaletteRow(accessTarget);
		paletteIndex = RegGetBackgroundPaletteColumn(accessTarget);

		// Record the source layer for this pixel
		if (imageBufferInfoEntry != 0)
		{
			imageBufferInfoEntry->pixelSource = PixelSource::Border;
		}
	}
	else if (insideActiveScanVertically && insideActiveScanHorizontally)
	{
		// If we're displaying a pixel in the active display region, determine the correct
		// palette index for this pixel.

		// Collect the pattern and priority data for this pixel from each of the various
		// layers.
		// Mapping (Pattern Name) data format:
		// -----------------------------------------------------------------
		// |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
		// |---------------------------------------------------------------|
		// |Pri|PalRow |VF |HF |              Pattern Number               |
		// -----------------------------------------------------------------
		// Pri:    Priority Bit
		// PalRow: The palette row number to use when displaying the pattern data
		// VF:     Vertical Flip
		// HF:     Horizontal Flip
		unsigned int paletteLineData[4];
		unsigned int paletteIndexData[4];
		bool layerPriority[4];

		// Decode the sprite mapping and pattern data
		layerPriority[LAYERINDEX_SPRITE] = false;
		paletteLineData[LAYERINDEX_SPRITE] = 0;
		paletteIndexData[LAYERINDEX_SPRITE] = 0;
		const SpritePixelBufferEntry& spritePixelBufferEntry = _spritePixelBuffer[_renderSpritePixelBufferAnalogRenderPlane][activeScanPixelIndex];
		if (spritePixelBufferEntry.entryWritten)
		{
			layerPriority[LAYERINDEX_SPRITE] = spritePixelBufferEntry.layerPriority;
			paletteLineData[LAYERINDEX_SPRITE] = spritePixelBufferEntry.paletteLine;
			paletteIndexData[LAYERINDEX_SPRITE] = spritePixelBufferEntry.paletteIndex;
		}

		// Decode the layer A mapping and pattern data
		unsigned int screenCellNo = activeScanPixelIndex / cellBlockSizeH;
		unsigned int screenColumnNo = screenCellNo / CellsPerColumn;
		bool windowEnabledAtCell = _renderWindowActiveCache[screenColumnNo];
		unsigned int mappingNumberWindow = { };
		unsigned int scrolledMappingNumberLayerA = { };
		unsigned int pixelNumberWindow = { };
		unsigned int scrolledPixelNumberLayerA = { };
		if (windowEnabledAtCell)
		{
			// Read the pixel data from the window plane
			mappingNumberWindow = ((cellBlockSizeH * CellsPerColumn) + activeScanPixelIndex) / cellBlockSizeH;
			pixelNumberWindow = ((cellBlockSizeH * CellsPerColumn) + activeScanPixelIndex) % cellBlockSizeH;
			const Data& windowMappingData = _renderMappingDataCacheLayerA[mappingNumberWindow];
			layerPriority[LAYERINDEX_LAYERA] = windowMappingData.GetBit(15);
			paletteLineData[LAYERINDEX_LAYERA] = windowMappingData.GetDataSegment(13, 2);
			paletteIndexData[LAYERINDEX_LAYERA] = DigitalRenderReadPixelIndex(_renderPatternDataCacheLayerA[mappingNumberWindow], windowMappingData.GetBit(11), pixelNumberWindow);
		}
		else
		{
			// Calculate the mapping number and pixel number within the layer
			scrolledMappingNumberLayerA = (((cellBlockSizeH * CellsPerColumn) + activeScanPixelIndex) - _renderLayerAHscrollPatternDisplacement) / cellBlockSizeH;
			scrolledPixelNumberLayerA = (((cellBlockSizeH * CellsPerColumn) + activeScanPixelIndex) - _renderLayerAHscrollPatternDisplacement) % cellBlockSizeH;

			// Take the window distortion bug into account. Due to an implementation quirk,
			// the real VDP apparently reads the window mapping and pattern data at the
			// start of the normal cell block reads, and doesn't use the left scrolled
			// 2-cell block to read window data. As a result, the VDP doesn't have enough
			// VRAM access slots left to handle a scrolled playfield when transitioning
			// from a left-aligned window to the scrolled layer A plane. As a result of
			// their implementation, the partially visible 2-cell region immediately
			// following the end of the window uses the mapping and pattern data of the
			// column that follows it. Our implementation here has a similar effect, except
			// that we would fetch the mapping and pattern data for the 2-cell block
			// immediately before it, IE from the last column of the window region. We
			// adjust the mapping number here to correctly fetch the next column mapping
			// data instead when a partially visible column follows a left-aligned window,
			// as would the real hardware.
			//##TODO## Confirm through hardware tests that window mapping and pattern data
			// is read after the left scrolled 2-cell block.
			unsigned int currentScreenColumnPixelIndex = activeScanPixelIndex - (cellBlockSizeH * CellsPerColumn * screenColumnNo);
			unsigned int distortedPixelCount = _renderLayerAHscrollPatternDisplacement + ((scrolledMappingNumberLayerA & 0x1) * cellBlockSizeH);
			if ((screenColumnNo > 0) && _renderWindowActiveCache[screenColumnNo-1] && (currentScreenColumnPixelIndex < distortedPixelCount))
			{
				scrolledMappingNumberLayerA += CellsPerColumn;
			}

			// Read the pixel data from the layer A plane
			const Data& layerAMappingData = _renderMappingDataCacheLayerA[scrolledMappingNumberLayerA];
			layerPriority[LAYERINDEX_LAYERA] = layerAMappingData.GetBit(15);
			paletteLineData[LAYERINDEX_LAYERA] = layerAMappingData.GetDataSegment(13, 2);
			paletteIndexData[LAYERINDEX_LAYERA] = DigitalRenderReadPixelIndex(_renderPatternDataCacheLayerA[scrolledMappingNumberLayerA], layerAMappingData.GetBit(11), scrolledPixelNumberLayerA);
		}

		// Decode the layer B mapping and pattern data
		unsigned int scrolledMappingNumberLayerB = (((cellBlockSizeH * CellsPerColumn) + activeScanPixelIndex) - _renderLayerBHscrollPatternDisplacement) / cellBlockSizeH;
		unsigned int scrolledPixelNumberLayerB = (((cellBlockSizeH * CellsPerColumn) + activeScanPixelIndex) - _renderLayerBHscrollPatternDisplacement) % cellBlockSizeH;
		const Data& layerBMappingData = _renderMappingDataCacheLayerB[scrolledMappingNumberLayerB];
		layerPriority[LAYERINDEX_LAYERB] = layerBMappingData.GetBit(15);
		paletteLineData[LAYERINDEX_LAYERB] = layerBMappingData.GetDataSegment(13, 2);
		paletteIndexData[LAYERINDEX_LAYERB] = DigitalRenderReadPixelIndex(_renderPatternDataCacheLayerB[scrolledMappingNumberLayerB], layerBMappingData.GetBit(11), scrolledPixelNumberLayerB);

		// Read the background palette settings
		layerPriority[LAYERINDEX_BACKGROUND] = false;
		paletteLineData[LAYERINDEX_BACKGROUND] = RegGetBackgroundPaletteRow(accessTarget);
		paletteIndexData[LAYERINDEX_BACKGROUND] = RegGetBackgroundPaletteColumn(accessTarget);

		// Determine if any of the palette index values for any of the layers indicate a
		// transparent pixel.
		//##TODO## Consider renaming and reversing the logic of these flags to match the
		// comment above. The name of "found pixel" isn't very descriptive, and in the case
		// of the sprite layer "isPixelOpaque" could be misleading when the sprite pixel is
		// being used as an operator in shadow/highlight mode. A flag with a name like
		// isPixelTransparent would be much more descriptive.
		bool foundSpritePixel = (paletteIndexData[LAYERINDEX_SPRITE] != 0);
		bool foundLayerAPixel = (paletteIndexData[LAYERINDEX_LAYERA] != 0);
		bool foundLayerBPixel = (paletteIndexData[LAYERINDEX_LAYERB] != 0);

		// Read the shadow/highlight mode settings. Note that hardware tests have confirmed
		// that changes to this register take effect immediately, at any point in a line.
		//##TODO## Confirm whether shadow highlight is active in border areas
		//##TODO## Confirm whether shadow highlight is active when the display is disabled
		bool shadowHighlightEnabled = RegGetSTE(accessTarget);
		bool spriteIsShadowOperator = (paletteLineData[LAYERINDEX_SPRITE] == 3) && (paletteIndexData[LAYERINDEX_SPRITE] == 15);
		bool spriteIsHighlightOperator = (paletteLineData[LAYERINDEX_SPRITE] == 3) && (paletteIndexData[LAYERINDEX_SPRITE] == 14);
		bool spriteIsNormalIntensity = (paletteIndexData[LAYERINDEX_SPRITE] == 14) && !spriteIsHighlightOperator;

		// Implement the layer removal debugging feature
		foundSpritePixel &= ((_enableSpriteHigh && _enableSpriteLow) || (_enableSpriteHigh && layerPriority[LAYERINDEX_SPRITE]) || (_enableSpriteLow && !layerPriority[LAYERINDEX_SPRITE]));
		foundLayerAPixel &= ((_enableLayerAHigh && _enableLayerALow) || (_enableLayerAHigh && layerPriority[LAYERINDEX_LAYERA]) || (_enableLayerALow && !layerPriority[LAYERINDEX_LAYERA]));
		foundLayerBPixel &= ((_enableLayerBHigh && _enableLayerBLow) || (_enableLayerBHigh && layerPriority[LAYERINDEX_LAYERB]) || (_enableLayerBLow && !layerPriority[LAYERINDEX_LAYERB]));

		//##NOTE## The following code is disabled, because we use a lookup table to cache
		// the result of layer priority calculations. This gives us a significant
		// performance boost. The code below is provided for future reference and debugging
		// purposes. This code should, in all instances, produce the same result as the
		// table lookup below.
		// Perform layer priority calculations, and determine the layer to use, as well as
		// the resulting state of the shadow and highlight bits.
		// unsigned int layerIndex;
		// bool shadow;
		// bool highlight;
		// CalculateLayerPriorityIndex(layerIndex, shadow, highlight, shadowHighlightEnabled, spriteIsShadowOperator, spriteIsHighlightOperator, foundSpritePixel, foundLayerAPixel, foundLayerBPixel, prioritySprite, priorityLayerA, priorityLayerB);

		// Encode the parameters for the layer priority calculation into an index value for
		// the priority lookup table.
		unsigned int priorityIndex = 0;
		priorityIndex |= (unsigned int)shadowHighlightEnabled << 8;
		priorityIndex |= (unsigned int)spriteIsShadowOperator << 7;
		priorityIndex |= (unsigned int)spriteIsHighlightOperator << 6;
		priorityIndex |= (unsigned int)foundSpritePixel << 5;
		priorityIndex |= (unsigned int)foundLayerAPixel << 4;
		priorityIndex |= (unsigned int)foundLayerBPixel << 3;
		priorityIndex |= (unsigned int)layerPriority[LAYERINDEX_SPRITE] << 2;
		priorityIndex |= (unsigned int)layerPriority[LAYERINDEX_LAYERA] << 1;
		priorityIndex |= (unsigned int)layerPriority[LAYERINDEX_LAYERB];

		// Lookup the pre-calculated layer priority from the lookup table. We use a lookup
		// table to eliminate branching, which should yield a significant performance
		// boost.
		unsigned int layerSelectionResult = _layerPriorityLookupTable[priorityIndex];

		// Extract the layer index, shadow, and highlight data from the combined result
		// returned from the layer priority lookup table.
		unsigned int layerIndex = layerSelectionResult & 0x03;
		shadow = (layerSelectionResult & 0x08) != 0;
		highlight = (layerSelectionResult & 0x04) != 0;

		// Read the palette line and index to use for the selected layer
		paletteLine = paletteLineData[layerIndex];
		paletteIndex = paletteIndexData[layerIndex];

		// If a sprite pixel uses palette index 14 on a palette row other than the last
		// one, it is always shown at normal intensity rather than being highlighted. This
		// has no real practical use, so it may be a hardware bug.
		if ((layerIndex == LAYERINDEX_SPRITE) && spriteIsNormalIntensity)
		{
			shadow = false;
			highlight = false;
		}

		// Record the source layer for this pixel
		if (imageBufferInfoEntry != 0)
		{
			switch (layerIndex)
			{
			case LAYERINDEX_SPRITE:
				imageBufferInfoEntry->pixelSource = PixelSource::Sprite;
				imageBufferInfoEntry->patternRowNo = spritePixelBufferEntry.patternRowNo;
				imageBufferInfoEntry->patternColumnNo = spritePixelBufferEntry.patternColumnNo;
				imageBufferInfoEntry->mappingVRAMAddress = spritePixelBufferEntry.spriteTableEntryAddress + 4;
				imageBufferInfoEntry->mappingData = spritePixelBufferEntry.spriteMappingData;
				imageBufferInfoEntry->spriteTableEntryNo = spritePixelBufferEntry.spriteTableEntryNo;
				imageBufferInfoEntry->spriteTableEntryAddress = spritePixelBufferEntry.spriteTableEntryAddress;
				imageBufferInfoEntry->spriteCellWidth = spritePixelBufferEntry.spriteCellWidth;
				imageBufferInfoEntry->spriteCellHeight = spritePixelBufferEntry.spriteCellHeight;
				imageBufferInfoEntry->spriteCellPosX = spritePixelBufferEntry.spriteCellPosX;
				imageBufferInfoEntry->spriteCellPosY = spritePixelBufferEntry.spriteCellPosY;
				break;
			case LAYERINDEX_LAYERA:
				if (windowEnabledAtCell)
				{
					imageBufferInfoEntry->pixelSource = PixelSource::Window;
					imageBufferInfoEntry->patternRowNo = _renderPatternDataCacheRowNoLayerA[mappingNumberWindow];
					imageBufferInfoEntry->patternColumnNo = pixelNumberWindow;
					imageBufferInfoEntry->mappingData = _renderMappingDataCacheLayerA[mappingNumberWindow];
					imageBufferInfoEntry->mappingVRAMAddress = _renderMappingDataCacheSourceAddressLayerA[mappingNumberWindow];
				}
				else
				{
					imageBufferInfoEntry->pixelSource = PixelSource::LayerA;
					imageBufferInfoEntry->patternRowNo = _renderPatternDataCacheRowNoLayerA[scrolledMappingNumberLayerB];
					imageBufferInfoEntry->patternColumnNo = scrolledPixelNumberLayerA;
					imageBufferInfoEntry->mappingData = _renderMappingDataCacheLayerA[scrolledMappingNumberLayerA];
					imageBufferInfoEntry->mappingVRAMAddress = _renderMappingDataCacheSourceAddressLayerA[scrolledMappingNumberLayerA];
				}
				break;
			case LAYERINDEX_LAYERB:
				imageBufferInfoEntry->pixelSource = PixelSource::LayerB;
				imageBufferInfoEntry->patternRowNo = _renderPatternDataCacheRowNoLayerB[scrolledMappingNumberLayerB];
				imageBufferInfoEntry->patternColumnNo = scrolledPixelNumberLayerB;
				imageBufferInfoEntry->mappingData = _renderMappingDataCacheLayerB[scrolledMappingNumberLayerB];
				imageBufferInfoEntry->mappingVRAMAddress = _renderMappingDataCacheSourceAddressLayerB[scrolledMappingNumberLayerB];
				break;
			case LAYERINDEX_BACKGROUND:
				imageBufferInfoEntry->pixelSource = PixelSource::Background;
				break;
			}
		}
	}

	// If we just determined the palette line and index for a pixel within the active scan
	// region of the screen, initialize the sprite pixel buffer at the corresponding pixel
	// location, so that it is clear and ready to receive new sprite data on the next line.
	//##TODO## Remove this old code
	// if(insideActiveScanVertically && insideActiveScanHorizontally)
	//{
	//	spritePixelBuffer[renderSpritePixelBufferAnalogRenderPlane][activeScanPixelIndex].entryWritten = false;
	//}

	//##TODO## Write a much longer comment here
	//##FIX## This comment doesn't actually reflect what we do right now
	// If a CRAM write has occurred at the same time as we're outputting this next
	// pixel, retrieve the value written to CRAM and output that value instead.
	//##TODO## Consider only advancing the CRAM buffer if we're passing the next write
	// time in this step. If we're not, there's no point doing the advance, since it
	// contains the same test we're doing here to decide if any work needs to be done.
	//##FIX## Correct a timing problem with all our buffers. Currently, writes to our
	// buffers are performed relative to the state update time, which factors in extra time
	// we may have rendered past the end of the timeslice in the last step. If we move past
	// the end of a timeslice, the following timeslice is shortened, and writes in that
	// timeslice are offset by the amount we ran over the last timeslice. We need to ensure
	// that the digital renderer knows about, and uses, these "stateLastUpdateMclk" values
	// to advance each timeslice, not the indicated timeslice length. This will ensure that
	// during the render process, values in the buffer will be committed at the same
	// relative time at which they were written.
	//##TODO## As part of the above, consider solving this issue more permanently, with an
	// upgrade to our timed buffers to roll writes past the end of a timeslice into the
	// next timeslice.
	if (_cramSession.writeInfo.exists && (_cramSession.nextWriteTime <= _renderDigitalMclkCycleProgress))
	{
		static const unsigned int paletteEntriesPerLine = 16;
		static const unsigned int paletteEntrySize = 2;
		unsigned int cramWriteAddress = _cramSession.writeInfo.writeAddress;
		paletteLine = (cramWriteAddress / paletteEntrySize) / paletteEntriesPerLine;
		paletteIndex = (cramWriteAddress / paletteEntrySize) % paletteEntriesPerLine;

		// Record the source layer for this pixel
		if (imageBufferInfoEntry != 0)
		{
			imageBufferInfoEntry->pixelSource = IS315_5313::PixelSource::CRAMWrite;
		}
	}

	// Record information on the selected palette entry for this pixel
	if (imageBufferInfoEntry != 0)
	{
		imageBufferInfoEntry->shadowHighlightEnabled = RegGetSTE(accessTarget);
		imageBufferInfoEntry->pixelIsShadowed = shadow;
		imageBufferInfoEntry->pixelIsHighlighted = highlight;
		imageBufferInfoEntry->paletteRow = paletteLine;
		imageBufferInfoEntry->paletteEntry = paletteIndex;
	}

	// Now that we've advanced the analog render cycle and handled CRAM write flicker,
	// advance the committed state of the CRAM buffer. If a write occurred to CRAM at the
	// same time as this pixel was being drawn, it will now have been committed to CRAM.
	_cram->AdvanceBySession(_renderDigitalMclkCycleProgress, _cramSession, _cramTimesliceCopy);

	// If we're drawing a pixel which is within the area of the screen we're rendering
	// pixel data for, output the pixel data to the image buffer.
	if (insidePixelBufferRegion)
	{
		// Constants
		static const unsigned int paletteEntriesPerLine = 16;
		static const unsigned int paletteEntrySize = 2;

		// Calculate the address of the colour value to read from the palette
		unsigned int paletteEntryAddress = (paletteIndex + (paletteLine * paletteEntriesPerLine)) * paletteEntrySize;

		// Read the target palette entry
		Data paletteData(16);
		paletteData = (unsigned int)(_cram->ReadCommitted(paletteEntryAddress+0) << 8) | (unsigned int)_cram->ReadCommitted(paletteEntryAddress+1);

		// Decode the target palette entry, and extract the individual 7-bit R, G, and B
		// intensity values.
		// -----------------------------------------------------------------
		// |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
		// |---------------------------------------------------------------|
		// | /   /   /   / |   Blue    | / |   Green   | / |    Red    | / |
		// -----------------------------------------------------------------
		unsigned int colorIntensityR = paletteData.GetDataSegment(1, 3);
		unsigned int colorIntensityG = paletteData.GetDataSegment(5, 3);
		unsigned int colorIntensityB = paletteData.GetDataSegment(9, 3);

		// If a reduced palette is in effect, due to bit 2 of register 1 being cleared,
		// only the lowest bit of each intensity value has any effect, and it selects
		// between half intensity and minimum intensity. Note that hardware tests have
		// shown that changes to this register take effect immediately, at any point in a
		// line.
		//##TODO## Confirm the interaction of shadow highlight mode with the palette
		// select bit.
		//##TODO## Confirm the mapping of intensity values when the palette select bit is
		// cleared.
		if (!RegGetPS(accessTarget))
		{
			colorIntensityR = (colorIntensityR & 0x01) << 2;
			colorIntensityG = (colorIntensityG & 0x01) << 2;
			colorIntensityB = (colorIntensityB & 0x01) << 2;
		}

		// Convert the palette data to a 32-bit RGBA triple and write it to the image
		// buffer
		//##TODO## As an optimization, use a combined lookup table for colour value
		// decoding, and eliminate the branching logic here.
		ImageBufferColorEntry& imageBufferEntry = *((ImageBufferColorEntry*)&_imageBuffer[_drawingImageBufferPlane][((renderAnalogCurrentRow * ImageBufferWidth) + renderAnalogCurrentPixel) * 4]);
		if (outputNothing)
		{
			imageBufferEntry.r = 0;
			imageBufferEntry.g = 0;
			imageBufferEntry.b = 0;
			imageBufferEntry.a = 0xFF;
		}
		else if (shadow == highlight)
		{
			imageBufferEntry.r = PaletteEntryTo8Bit[colorIntensityR];
			imageBufferEntry.g = PaletteEntryTo8Bit[colorIntensityG];
			imageBufferEntry.b = PaletteEntryTo8Bit[colorIntensityB];
			imageBufferEntry.a = 0xFF;
		}
		else if (shadow && !highlight)
		{
			imageBufferEntry.r = PaletteEntryTo8BitShadow[colorIntensityR];
			imageBufferEntry.g = PaletteEntryTo8BitShadow[colorIntensityG];
			imageBufferEntry.b = PaletteEntryTo8BitShadow[colorIntensityB];
			imageBufferEntry.a = 0xFF;
		}
		else if (highlight && !shadow)
		{
			imageBufferEntry.r = PaletteEntryTo8BitHighlight[colorIntensityR];
			imageBufferEntry.g = PaletteEntryTo8BitHighlight[colorIntensityG];
			imageBufferEntry.b = PaletteEntryTo8BitHighlight[colorIntensityB];
			imageBufferEntry.a = 0xFF;
		}

		// Record information on the output colour for this pixel
		if (imageBufferInfoEntry != 0)
		{
			imageBufferInfoEntry->colorComponentR = colorIntensityR;
			imageBufferInfoEntry->colorComponentG = colorIntensityG;
			imageBufferInfoEntry->colorComponentB = colorIntensityB;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::DigitalRenderReadHscrollData(unsigned int screenRowNumber, unsigned int hscrollDataBase, bool hscrState, bool lscrState, unsigned int& layerAHscrollPatternDisplacement, unsigned int& layerBHscrollPatternDisplacement, unsigned int& layerAHscrollMappingDisplacement, unsigned int& layerBHscrollMappingDisplacement) const
{
	// Calculate the address of the hscroll data to read for this line
	unsigned int hscrollDataAddress = hscrollDataBase;
	//##TODO## Based on the EA logo for Populous, it appears that the state of LSCR is
	// ignored when HSCR is not set. We should confirm this on hardware.
	if (hscrState)
	{
		static const unsigned int hscrollDataPairSize = 4;
		hscrollDataAddress += lscrState? (screenRowNumber * hscrollDataPairSize): (((screenRowNumber / RenderDigitalBlockPixelSizeY) * RenderDigitalBlockPixelSizeY) * hscrollDataPairSize);
	}

	// Read the hscroll data for this line
	//##TODO## Confirm the way scrolling data is interpreted through hardware tests. Eg,
	// does -1 actually scroll to the left by one place, or are 0 and -1 equivalent?
	//##TODO## According to the official documentation, the upper 6 bits of the hscroll
	// data are unused, and are allowed to be used by software to store whatever values
	// they want. Confirm this on hardware.
	unsigned int layerAHscrollOffset = ((unsigned int)_vram->ReadCommitted(hscrollDataAddress+0) << 8) | (unsigned int)_vram->ReadCommitted(hscrollDataAddress+1);
	unsigned int layerBHscrollOffset = ((unsigned int)_vram->ReadCommitted(hscrollDataAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(hscrollDataAddress+3);

	// Break the hscroll data into its two component parts. The lower 4 bits represent a
	// displacement into the 2-cell column, or in other words, the displacement of the
	// starting pixel within each column, while the upper 6 bits represent an offset for
	// the column mapping data itself.
	// -----------------------------------------
	// | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// |---------------------------------------|
	// |  Column Shift Value   | Displacement  |
	// -----------------------------------------
	layerAHscrollPatternDisplacement = (layerAHscrollOffset & 0x00F);
	layerAHscrollMappingDisplacement = (layerAHscrollOffset & 0x3F0) >> 4;
	layerBHscrollPatternDisplacement = (layerBHscrollOffset & 0x00F);
	layerBHscrollMappingDisplacement = (layerBHscrollOffset & 0x3F0) >> 4;
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::DigitalRenderReadVscrollData(unsigned int screenColumnNumber, unsigned int layerNumber, bool vscrState, bool interlaceMode2Active, unsigned int& layerVscrollPatternDisplacement, unsigned int& layerVscrollMappingDisplacement, Data& vsramReadCache) const
{
	// Calculate the address of the vscroll data to read for this block
	static const unsigned int vscrollDataLayerCount = 2;
	static const unsigned int vscrollDataEntrySize = 2;
	unsigned int vscrollDataAddress = vscrState? (screenColumnNumber * vscrollDataLayerCount * vscrollDataEntrySize) + (layerNumber * vscrollDataEntrySize): (layerNumber * vscrollDataEntrySize);

	//##NOTE## This implements what appears to be the correct behaviour for handling reads
	// past the end of the VSRAM buffer during rendering. This can occur when horizontal
	// scrolling is applied along with vertical scrolling, in which case the leftmost
	// column can be reading data from a screen column of -1, wrapping around to the end of
	// the VSRAM buffer. In this case, the last successfully read value from the VSRAM
	// appears to be used as the read value. This also applies when performing manual reads
	// from VSRAM externally using the data port. See data port reads from VSRAM for more
	// info.
	//##TODO## This needs more through hardware tests, to definitively confirm the correct
	// behaviour.
	if (vscrollDataAddress < 0x50)
	{
		// Read the vscroll data for this line. Note only the lower 10 bits are
		// effective, or the lower 11 bits in the case of interlace mode 2, due to the
		// scrolled address being wrapped to lie within the total field boundaries,
		// which never exceed 128 blocks.
		vsramReadCache = ((unsigned int)_vsram->ReadCommitted(vscrollDataAddress+0) << 8) | (unsigned int)_vsram->ReadCommitted(vscrollDataAddress+1);
	}
	else
	{
		//##FIX## This is a temporary patch until we complete our hardware testing on the
		// behaviour of VSRAM. Hardware tests do seem to confirm that when the VSRAM read
		// process passes into the undefined upper region of VSRAM, the returned value is
		// the ANDed result of the last two entries in VSRAM.
		vsramReadCache = ((unsigned int)_vsram->ReadCommitted(0x4C+0) << 8) | (unsigned int)_vsram->ReadCommitted(0x4C+1);
		vsramReadCache &= ((unsigned int)_vsram->ReadCommitted(0x4E+0) << 8) | (unsigned int)_vsram->ReadCommitted(0x4E+1);
	}

	// Break the vscroll data into its two component parts. The format of the vscroll data
	// varies depending on whether interlace mode 2 is active. When interlace mode 2 is not
	// active, the vscroll data is interpreted as a 10-bit value, where the lower 3 bits
	// represent a vertical shift on the pattern line for the selected block mapping, or in
	// other words, the displacement of the starting row within each pattern, while the
	// upper 7 bits represent an offset for the mapping data itself, like so:
	// ------------------------------------------
	// | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0  |
	// |----------------------------------------|
	// |    Column Shift Value     |Displacement|
	// ------------------------------------------
	// Where interlace mode 2 is active, pattern data is 8x16 pixels, not 8x8 pixels. In
	// this case, the vscroll data is treated as an 11-bit value, where the lower 4 bits
	// give the row offset, and the upper 7 bits give the mapping offset, like so:
	// ---------------------------------------------
	// |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// |-------------------------------------------|
	// |    Column Shift Value     | Displacement  |
	// ---------------------------------------------
	// Note that the unused upper bits in the vscroll data are simply discarded, since they
	// fall outside the maximum virtual playfield size for the mapping data. Since the
	// virtual playfield wraps, this means they have no effect.
	layerVscrollPatternDisplacement = interlaceMode2Active? vsramReadCache.GetDataSegment(0, 4): vsramReadCache.GetDataSegment(0, 3);
	layerVscrollMappingDisplacement = interlaceMode2Active? vsramReadCache.GetDataSegment(4, 7): vsramReadCache.GetDataSegment(3, 7);
}

//----------------------------------------------------------------------------------------------------------------------
// This function performs all the necessary calculations to determine which mapping data to
// read for a given playfield position, and reads the corresponding mapping data pair from
// VRAM. The calculations performed appear to produce the same result as the real VDP
// hardware under all modes and settings, including when invalid scroll size modes are
// used.
//
// The following comments are provided as a supplement to the comments within this
// function, and show how the internally calculated row and column numbers are combined
// with the mapping base address data to produce a final VRAM address for the mapping
// block. All possible combinations of screen mode settings are shown, including invalid
// modes (VSZ="10" or HSZ="10"). Note that invalid combinations of screen mode settings are
// not shown, since invalid combinations never actually occur, due to the vertical screen
// mode being adjusted based on the horizontal screen mode, as outlined in the function
// comments below.
//
// Officially supported screen mode settings. Note that the lower two bits of the resulting
// address are masked before the address is used.
// Mapping data VRAM address (HSZ=00 VSZ=00):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr | 0 | 0 |        Row        |       Column      | 0 | (Row Shift Count = 6)
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=01 VSZ=00):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr | 0 |        Row        |         Column        | 0 | (Row Shift Count = 7)
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=11 VSZ=00):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr |        Row        |           Column          | 0 | (Row Shift Count = 8)
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=00 VSZ=01):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr | 0 |          Row          |       Column      | 0 | (Row Shift Count = 6)
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=01 VSZ=01):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr |          Row          |         Column        | 0 | (Row Shift Count = 7)
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=00 VSZ=11):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr |            Row            |       Column      | 0 | (Row Shift Count = 6)
// -----------------------------------------------------------------
//
// Officially unsupported screen modes. In this case, the row and column data may be
// interleaved, and the row shift count may be 0, as shown below:
// Mapping data VRAM address (HSZ=00 VSZ=10):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// |           |Row| 0 |        Row        |                       | (Row Shift Count = 6)
// |           -------------------------------------------------   |
// |                                       |       Column      |   |
// |---------------------------------------------------------------|
// | Base Addr |Row| 0 |        Row        |       Column      | 0 |
// -----------------------------------------------------------------
//##FIX## Hardware tests have shown the two cases below to be incorrect. It appears the
// upper bit of the column data is never applied, and the row is never incremented when the
// invalid horizontal mode is active.
// Mapping data VRAM address (HSZ=10 VSZ=00):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// |                                           |        Row        | (Row Shift Count = 0)
// |                                       ------------------------|
// |                                       |       Column      |   |
// |---------------------------------------------------------------|
// | Base Addr | 0 | 0 | 0 | 0 | 0 |Col| 0 |       Column      |Row|
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=10 VSZ=10):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// |                                   |Row| 0 |        Row        | (Row Shift Count = 0)
// |                               --------------------------------|
// |                               |Col| 0 |       Column      |   |
// |---------------------------------------------------------------|
// | Base Addr | 0 | 0 | 0 | 0 | 0 |Col|Row|       Column      |Row|
// -----------------------------------------------------------------
//##TODO## Implement the correct mappings, which are as follows:
// Mapping data VRAM address (HSZ=10 VSZ=00):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr | 0 | 0 | 0 | 0 | 0 | 0 | 0 |       Column      | 0 |
// -----------------------------------------------------------------
// Mapping data VRAM address (HSZ=10 VSZ=10):
// -----------------------------------------------------------------
// | 15| 14| 13| 12| 11| 10| 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
// |---------------------------------------------------------------|
// | Base Addr | 0 | 0 | 0 | 0 | 0 | 0 | 0 |       Column      | 0 |
// -----------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::DigitalRenderCalculateMappingVRAMAddess(unsigned int screenRowNumber, unsigned int screenColumnNumber, bool interlaceMode2Active, unsigned int nameTableBaseAddress, unsigned int layerHscrollMappingDisplacement, unsigned int layerVscrollMappingDisplacement, unsigned int layerVscrollPatternDisplacement, unsigned int hszState, unsigned int vszState)
{
	// The existence and contents of this table have been determined through analysis of
	// the behaviour of the VDP when invalid field size settings are selected. In
	// particular, the third table entry of 0 is used when the invalid HSZ mode of "10" is
	// selected. This causes the row and column to overlap when building the final address
	// value. The way the address is built in these circumstances can only be logically
	// explained by a lookup table being used for the row shift count, with the third entry
	// being set to 0, as we have implemented here.
	//##FIX## Hardware tests have shown this is not actually the case
	static const unsigned int rowShiftCountTable[4] = {6, 7, 0, 8};

	// The following calculation limits the vertical playfield size, based on the
	// horizontal playfield size. This calculation is quite simple in hardware, but looks
	// more complicated in code than it really is. Basically, the upper bit of the vertical
	// scroll mode is run through a NAND operation with the lower bit of the horizontal
	// scroll mode, and likewise, the lower bit of the vertical scroll mode is run through
	// a NAND operation with the upper bit of the horizontal scroll mode. This limits the
	// vertical scroll mode in the exact same way the real hardware does, including when
	// invalid scroll modes are being used.
	unsigned int screenSizeModeH = hszState;
	unsigned int screenSizeModeV = ((vszState & 0x1) & ((~hszState & 0x02) >> 1)) | ((vszState & 0x02) & ((~hszState & 0x01) << 1));

	// Build the playfield block masks. These masks ultimately determine the boundaries of
	// the playfield horizontally and vertically. Hardware testing and analysis on the
	// scrolling behaviour of the VDP indicate that these masks are built by mapping the
	// HSZ and VSZ bits to the upper bits of a 7-bit mask value, as shown below. Note that
	// the invalid mode setting of "10" results in a mask where bit 7 is set, and bit 6 is
	// unset. This has been confirmed through hardware tests.
	//##TODO## Test this on hardware.
	//##TODO## Test and confirm how the window distortion bug interacts with this block
	// mapping selection process.
	unsigned int playfieldBlockMaskX = (screenSizeModeH << 5) | 0x1F;
	unsigned int playfieldBlockMaskY = (screenSizeModeV << 5) | 0x1F;

	//##TODO## Update this comment
	// Calculate the row and column numbers for the mapping data. This is simply done by
	// converting the calculated playfield position from a pixel index into a block index,
	// then masking the X and Y coordinates by the horizontal and vertical block masks.
	// This gives us a row and column number, wrapped to the playfield boundaries.
	//##TODO## We want to keep this shift method for calculating the row and column,
	// rather than using division, but we should be using a constant, or at least
	// explaining why the magic number "3" is being used.
	//##TODO## Update these comments
	//##TODO## Document why we add the horizontal scroll value, but subtract the vertical
	// scroll value.
	const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
	unsigned int mappingDataRowNumber = (((screenRowNumber + layerVscrollPatternDisplacement) / rowsPerTile) + layerVscrollMappingDisplacement) & playfieldBlockMaskY;
	unsigned int mappingDataColumnNumber = ((screenColumnNumber - layerHscrollMappingDisplacement) * CellsPerColumn) & playfieldBlockMaskX;

	// Based on the horizontal playfield mode, lookup the row shift count to use when
	// building the final mapping data address value. The column shift count is always
	// fixed to 1.
	unsigned int rowShiftCount = rowShiftCountTable[screenSizeModeH];
	static const unsigned int columnShiftCount = 1;

	// Calculate the final mapping data address. Note that the row number is masked with
	// the inverted mask for the column number, so that row data is only allowed to appear
	// where column data is not allowed to appear. This is based on the observed behaviour
	// of the system, as is critical in order to correctly emulate the scrolling behaviour
	// where an invalid horizontal scroll mode of "10" is applied. In this case, the row
	// data can be interleaved with the column data, since the row shift count under this
	// mode is 0.
	unsigned int mappingDataAddress = nameTableBaseAddress | ((mappingDataRowNumber << rowShiftCount) & (~playfieldBlockMaskX << columnShiftCount)) | (mappingDataColumnNumber << columnShiftCount);

	// Mask the lower two bits of the mapping data address, to align the mapping address
	// with a 4-byte boundary. The VDP can only read data from the VRAM in aligned 4-byte
	// blocks, so the lower two bits of the address are ineffective. We read a pair of
	// 2-byte block mappings from the masked address.
	mappingDataAddress &= 0xFFFC;

	//##FIX## This is a temporary workaround for the inaccurate results we get when a
	// horizontal mode of "10" is used. See notes above for more info. We need to determine
	// how and why the hardware behaves in this manner, to see if there's a simpler
	// implementation we can make where this behaviour is a natural side effect.
	if (screenSizeModeH == 2)
	{
		mappingDataAddress &= 0xFF3F;
	}

	// Return the calculated mapping data address to the caller
	return mappingDataAddress;
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::DigitalRenderBuildSpriteList(unsigned int screenRowNumber, bool interlaceMode2Active, bool screenModeRS1Active, unsigned int& nextTableEntryToRead, bool& spriteSearchComplete, bool& spriteOverflow, unsigned int& spriteDisplayCacheEntryCount, std::vector<SpriteDisplayCacheEntry>& spriteDisplayCache) const
{
	if (!spriteSearchComplete && !spriteOverflow)
	{
		static const unsigned int spriteCacheEntrySize = 4;
		const unsigned int spriteAttributeTableSize = (screenModeRS1Active)? 80: 64;
		//static const unsigned int spritePosScreenStartH = 0x80;
		const unsigned int spritePosScreenStartV = (interlaceMode2Active)? 0x100: 0x80;
		const unsigned int spritePosBitCountV = (interlaceMode2Active)? 10: 9;
		const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
		const unsigned int renderSpriteDisplayCacheSize = (screenModeRS1Active)? 20: 16;
		//const unsigned int renderSpriteCellDisplayCacheSize = (screenModeRS1Active)? 40: 32;

		// Calculate the address in the sprite cache of the next sprite to read data for
		unsigned int spriteCacheAddress = (nextTableEntryToRead * spriteCacheEntrySize);

		// Read all available data on the next sprite from the sprite cache
		Data spriteVPosData(16);
		Data spriteSizeAndLinkData(16);
		spriteVPosData = ((unsigned int)_spriteCache->ReadCommitted(spriteCacheAddress+0) << 8) | (unsigned int)_spriteCache->ReadCommitted(spriteCacheAddress+1);
		spriteSizeAndLinkData = ((unsigned int)_spriteCache->ReadCommitted(spriteCacheAddress+2) << 8) | (unsigned int)_spriteCache->ReadCommitted(spriteCacheAddress+3);

		// Calculate the width and height of this sprite in cells
		unsigned int spriteHeightInCells = spriteSizeAndLinkData.GetDataSegment(8, 2) + 1;

		// Calculate the relative position of the current active display line in sprite
		// space.
		//##TODO## Lay this code out better, and provide more comments on what's being
		// done to support interlace mode 2.
		//##TODO## Handle interlace mode 2 properly. In interlace mode 2, the current
		// screen row number is effectively doubled, with the current state of the odd flag
		// used as the LSB of the line number. We need to do that here.
		unsigned int currentScreenRowInSpriteSpace = screenRowNumber + spritePosScreenStartV;
		if (interlaceMode2Active)
		{
			currentScreenRowInSpriteSpace = (screenRowNumber * 2) + spritePosScreenStartV;
			if (_renderDigitalOddFlagSet)
			{
				currentScreenRowInSpriteSpace += 1;
			}
		}

		// Calculate the vertical position of the sprite, discarding any unused bits.
		Data spriteVPos(spritePosBitCountV);
		spriteVPos = spriteVPosData;

		// If this next sprite is within the current display row, add it to the list of
		// sprites to display on this line.
		unsigned int spriteHeightInPixels = spriteHeightInCells * rowsPerTile;
		if ((spriteVPos <= currentScreenRowInSpriteSpace) && ((spriteVPos + spriteHeightInPixels) > currentScreenRowInSpriteSpace))
		{
			// We perform a check for a sprite overflow here. If we exceed the maximum
			// number of sprites for this line, we set the sprite overflow flag, otherwise
			// we load all the sprite data from the sprite cache for this sprite.
			if (spriteDisplayCacheEntryCount < renderSpriteDisplayCacheSize)
			{
				spriteDisplayCache[spriteDisplayCacheEntryCount].spriteTableIndex = nextTableEntryToRead;
				spriteDisplayCache[spriteDisplayCacheEntryCount].spriteRowIndex = (currentScreenRowInSpriteSpace - spriteVPos.GetData());
				spriteDisplayCache[spriteDisplayCacheEntryCount].vpos = spriteVPosData;
				spriteDisplayCache[spriteDisplayCacheEntryCount].sizeAndLinkData = spriteSizeAndLinkData;
				++spriteDisplayCacheEntryCount;
			}
			else
			{
				spriteOverflow = true;
			}
		}

		// Use the link data to determine which sprite table entry to read data for next.
		// The sprite search is terminated if we encounter a sprite with a link data value
		// of 0, or if a link data value is specified which is outside the bounds of the
		// sprite table, based on the current screen mode settings.
		nextTableEntryToRead = spriteSizeAndLinkData.GetDataSegment(0, 7);
		if ((nextTableEntryToRead == 0) || (nextTableEntryToRead >= spriteAttributeTableSize))
		{
			spriteSearchComplete = true;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::DigitalRenderBuildSpriteCellList(const HScanSettings& hscanSettings, const VScanSettings& vscanSettings, unsigned int spriteDisplayCacheIndex, unsigned int spriteTableBaseAddress, bool interlaceMode2Active, bool screenModeRS1Active, bool& spriteDotOverflow, SpriteDisplayCacheEntry& spriteDisplayCacheEntry, unsigned int& spriteCellDisplayCacheEntryCount, std::vector<SpriteCellDisplayCacheEntry>& spriteCellDisplayCache) const
{
	if (!spriteDotOverflow)
	{
		//##TODO## Tidy up this list of constants
		//static const unsigned int spriteCacheEntrySize = 4;
		static const unsigned int spriteTableEntrySize = 8;
		//const unsigned int spriteAttributeTableSize = (screenModeRS1Active)? 80: 64;
		static const unsigned int spritePosScreenStartH = 0x80;
		const unsigned int spritePosScreenStartV = (interlaceMode2Active)? 0x100: 0x80;
		const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
		//const unsigned int renderSpriteDisplayCacheSize = (screenModeRS1Active)? 20: 16;
		const unsigned int renderSpriteCellDisplayCacheSize = (screenModeRS1Active)? 40: 32;

		// Calculate the address in VRAM of this sprite table entry
		unsigned int spriteTableEntryAddress = spriteTableBaseAddress + (spriteDisplayCacheEntry.spriteTableIndex * spriteTableEntrySize);

		// Read all remaining data for the sprite from the sprite attribute table in VRAM.
		// The first 4 bytes of the sprite attribute entry have already been loaded from
		// the internal sprite cache.
		//##TODO## Perform hardware tests, to confirm if the data from the sprite cache is
		// buffered after it is read the first time while parsing the sprite table to
		// determine the list of sprites on the current line, or if the data is read again
		// from the cache during the mapping decoding.
		Data spriteMappingData(16);
		Data spriteHPosData(16);
		spriteMappingData = ((unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+4) << 8) | (unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+5);
		spriteHPosData = ((unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+6) << 8) | (unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+7);

		// Load the remaining data into the sprite display cache
		spriteDisplayCacheEntry.mappingData = spriteMappingData;
		spriteDisplayCacheEntry.hpos = spriteHPosData;

		// Calculate the width and height of this sprite in cells
		unsigned int spriteWidthInCells = spriteDisplayCacheEntry.sizeAndLinkData.GetDataSegment(10, 2) + 1;
		unsigned int spriteHeightInCells = spriteDisplayCacheEntry.sizeAndLinkData.GetDataSegment(8, 2) + 1;

		// Extract the vertical flip and horizontal flip flags from the sprite mapping.
		// Unlike vertical and horizontal flip for normal pattern cells in the scroll
		// layers, when horizontal flip or vertical flip is applied to a sprite, the entire
		// sprite is flipped, so that the cell order itself is reversed, as well as the
		// order of the pixels within each cell. We extract the vertical flip and
		// horizontal flip flags here so we can emulate that behaviour.
		bool hflip = spriteMappingData.GetBit(11);
		bool vflip = spriteMappingData.GetBit(12);

		// Calculate the cell row number, and pattern row number within the cell, which
		// fall on the current display line for this sprite.
		unsigned int spriteCellRowNumber = spriteDisplayCacheEntry.spriteRowIndex / rowsPerTile;
		unsigned int spriteCellPatternRowNumber = spriteDisplayCacheEntry.spriteRowIndex % rowsPerTile;

		// Apply vertical flip to the sprite cell number if vertical flip is active. Note
		// that we do not invert the pattern row number here, just the cell row number. All
		// we're doing at this stage of sprite processing is decoding the cells within the
		// sprite that fall on the current display line. Vertical flip is applied to the
		// pattern row within a cell at a later stage of sprite processing.
		if (vflip)
		{
			spriteCellRowNumber = ((spriteHeightInCells - 1) - spriteCellRowNumber);
		}

		// Add details of each cell in this sprite on the current display line to the
		// internal sprite pattern render list.
		for (unsigned int i = 0; i < spriteWidthInCells; ++i)
		{
			// Record sprite boundary information for sprite boxing support if requested
			if (_videoEnableSpriteBoxing && (spriteCellDisplayCacheEntryCount < renderSpriteCellDisplayCacheSize))
			{
				std::unique_lock<std::mutex> spriteLock(_spriteBoundaryMutex[_drawingImageBufferPlane]);

				// Calculate the position of this sprite relative to the screen
				static const unsigned int cellWidthInPixels = 8;
				const unsigned int spritePosBitCountV = (interlaceMode2Active)? 10: 9;
				Data spritePosH(9, spriteDisplayCacheEntry.hpos.GetData());
				Data spritePosV(spritePosBitCountV, spriteDisplayCacheEntry.vpos.GetData());
				int spriteHeightDivider = (!interlaceMode2Active)? 1: 2;
				int spritePosXInScreenSpace = ((int)spritePosH.GetData() - (int)spritePosScreenStartH) + (int)hscanSettings.leftBorderPixelCount;
				int spritePosYInScreenSpace = (((int)spritePosV.GetData() - (int)spritePosScreenStartV) / spriteHeightDivider) + (int)vscanSettings.topBorderLineCount;

				// If this is the first cell column for the sprite, draw a horizontal line
				// down the left boundary of the sprite.
				if (i == 0)
				{
					SpriteBoundaryLineEntry spriteBoundaryLineEntry;
					spriteBoundaryLineEntry.linePosXStart = spritePosXInScreenSpace;
					spriteBoundaryLineEntry.linePosXEnd = spritePosXInScreenSpace;
					spriteBoundaryLineEntry.linePosYStart = spritePosYInScreenSpace + ((int)spriteDisplayCacheEntry.spriteRowIndex / spriteHeightDivider);
					spriteBoundaryLineEntry.linePosYEnd = spritePosYInScreenSpace + (((int)spriteDisplayCacheEntry.spriteRowIndex / spriteHeightDivider) + 1);
					_imageBufferSpriteBoundaryLines[_drawingImageBufferPlane].push_back(spriteBoundaryLineEntry);
				}

				// If this is the last cell column for the sprite, draw a horizontal line
				// down the right boundary of the sprite.
				if (((i + 1) == spriteWidthInCells) || ((spriteCellDisplayCacheEntryCount + 1) == renderSpriteCellDisplayCacheSize))
				{
					SpriteBoundaryLineEntry spriteBoundaryLineEntry;
					spriteBoundaryLineEntry.linePosXStart = spritePosXInScreenSpace + (int)((i + 1) * cellWidthInPixels);
					spriteBoundaryLineEntry.linePosXEnd = spritePosXInScreenSpace + (int)((i + 1) * cellWidthInPixels);
					spriteBoundaryLineEntry.linePosYStart = spritePosYInScreenSpace + ((int)spriteDisplayCacheEntry.spriteRowIndex / spriteHeightDivider);
					spriteBoundaryLineEntry.linePosYEnd = spritePosYInScreenSpace + (((int)spriteDisplayCacheEntry.spriteRowIndex / spriteHeightDivider) + 1);
					_imageBufferSpriteBoundaryLines[_drawingImageBufferPlane].push_back(spriteBoundaryLineEntry);
				}

				// If this is the first line for the sprite, draw a horizontal line across
				// the top boundary of the sprite.
				if ((spriteDisplayCacheEntry.spriteRowIndex == 0) || (interlaceMode2Active && (spriteDisplayCacheEntry.spriteRowIndex == 1)))
				{
					SpriteBoundaryLineEntry spriteBoundaryLineEntry;
					spriteBoundaryLineEntry.linePosXStart = spritePosXInScreenSpace + (int)(i * cellWidthInPixels);
					spriteBoundaryLineEntry.linePosXEnd = spritePosXInScreenSpace + (int)((i + 1) * cellWidthInPixels);
					spriteBoundaryLineEntry.linePosYStart = spritePosYInScreenSpace;
					spriteBoundaryLineEntry.linePosYEnd = spritePosYInScreenSpace;
					_imageBufferSpriteBoundaryLines[_drawingImageBufferPlane].push_back(spriteBoundaryLineEntry);
				}

				// If this is the last line for the sprite, draw a horizontal line across
				// the bottom boundary of the sprite.
				if (((spriteDisplayCacheEntry.spriteRowIndex + 1) == (spriteHeightInCells * rowsPerTile)) || (interlaceMode2Active && ((spriteDisplayCacheEntry.spriteRowIndex + 2) >= (spriteHeightInCells * rowsPerTile))))
				{
					SpriteBoundaryLineEntry spriteBoundaryLineEntry;
					spriteBoundaryLineEntry.linePosXStart = spritePosXInScreenSpace + (int)(i * cellWidthInPixels);
					spriteBoundaryLineEntry.linePosXEnd = spritePosXInScreenSpace + (int)((i + 1) * cellWidthInPixels);
					spriteBoundaryLineEntry.linePosYStart = spritePosYInScreenSpace + (((int)spriteDisplayCacheEntry.spriteRowIndex / spriteHeightDivider) + 1);
					spriteBoundaryLineEntry.linePosYEnd = spritePosYInScreenSpace + (((int)spriteDisplayCacheEntry.spriteRowIndex / spriteHeightDivider) + 1);
					_imageBufferSpriteBoundaryLines[_drawingImageBufferPlane].push_back(spriteBoundaryLineEntry);
				}
			}

			// We perform a check for a sprite dot overflow here. If each sprite is 2 cells
			// wide, and the maximum number of sprites for a line are present, we generate
			// the exact maximum number of sprite dots per line. If sprite widths are 3
			// cells or wider, we can exceed the maximum sprite dot count at this point.
			if (spriteCellDisplayCacheEntryCount < renderSpriteCellDisplayCacheSize)
			{
				// Record the reference from this individual sprite cell back to the sprite
				// display cache entry that holds the mapping data for the cell.
				spriteCellDisplayCache[spriteCellDisplayCacheEntryCount].spriteDisplayCacheIndex = spriteDisplayCacheIndex;

				// Calculate the cell offset into the sprite pattern data for this cell,
				// taking into account horizontal flip. Note that pattern cells for sprites
				// are ordered in columns, not rows. The pattern data for the top left cell
				// appears first, followed by the pattern data for the second row on the
				// leftmost column, and so forth to the end of that column, followed by the
				// pattern data for the topmost cell in the second column, and so on.
				unsigned int spriteCellHorizontalOffset = (hflip)? (spriteWidthInCells - 1) - i: i;
				SpriteCellDisplayCacheEntry& cellCacheEntry = spriteCellDisplayCache[spriteCellDisplayCacheEntryCount];
				cellCacheEntry.spriteWidthInCells = spriteWidthInCells;
				cellCacheEntry.spriteHeightInCells = spriteHeightInCells;
				cellCacheEntry.patternCellOffsetX = spriteCellHorizontalOffset;
				cellCacheEntry.patternCellOffsetY = spriteCellRowNumber;
				cellCacheEntry.patternRowOffset = spriteCellPatternRowNumber;
				cellCacheEntry.spriteCellColumnNo = i;

				// Record debug info on this sprite cell buffer entry, so that we can
				// reverse the render pipeline later for debug purposes and determine the
				// sprite that generated this pixel.
				cellCacheEntry.spriteTableIndex = spriteDisplayCacheEntry.spriteTableIndex;
				cellCacheEntry.spriteTableEntryAddress = spriteTableEntryAddress;

				// Advance to the next available entry in the sprite cell cache
				++spriteCellDisplayCacheEntryCount;
			}
			else
			{
				spriteDotOverflow = true;
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::DigitalRenderReadPixelIndex(const Data& patternRow, bool horizontalFlip, unsigned int pixelIndex) const
{
	static const unsigned int patternDataPixelEntryBitCount = 4;
	if (!horizontalFlip)
	{
		// Pattern data row format (no horizontal flip):
		// ---------------------------------------------------------------------------------------------------------------------------------
		// |31 |30 |29 |28 |27 |26 |25 |24 |23 |22 |21 |20 |19 |18 |17 |16 |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
		// |-------------------------------------------------------------------------------------------------------------------------------|
		// |    Pixel 1    |    Pixel 2    |    Pixel 3    |    Pixel 4    |    Pixel 5    |    Pixel 6    |    Pixel 7    |    Pixel 8    |
		// ---------------------------------------------------------------------------------------------------------------------------------
		return patternRow.GetDataSegment(((cellBlockSizeH - 1) - pixelIndex) * patternDataPixelEntryBitCount, 4);
	}
	else
	{
		// Pattern data row format (with horizontal flip):
		// ---------------------------------------------------------------------------------------------------------------------------------
		// |31 |30 |29 |28 |27 |26 |25 |24 |23 |22 |21 |20 |19 |18 |17 |16 |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
		// |-------------------------------------------------------------------------------------------------------------------------------|
		// |    Pixel 8    |    Pixel 7    |    Pixel 6    |    Pixel 5    |    Pixel 4    |    Pixel 3    |    Pixel 2    |    Pixel 1    |
		// ---------------------------------------------------------------------------------------------------------------------------------
		return patternRow.GetDataSegment(pixelIndex * patternDataPixelEntryBitCount, 4);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::CalculateLayerPriorityIndex(unsigned int& layerIndex, bool& shadow, bool& highlight, bool shadowHighlightEnabled, bool spriteIsShadowOperator, bool spriteIsHighlightOperator, bool foundSpritePixel, bool foundLayerAPixel, bool foundLayerBPixel, bool prioritySprite, bool priorityLayerA, bool priorityLayerB) const
{
	// Initialize the shadow/highlight flags
	shadow = false;
	highlight = false;

	// Perform layer priority calculations
	if (!shadowHighlightEnabled)
	{
		// Perform standard layer priority calculations
		if (foundSpritePixel && prioritySprite)
		{
			layerIndex = LAYERINDEX_SPRITE;
		}
		else if (foundLayerAPixel && priorityLayerA)
		{
			layerIndex = LAYERINDEX_LAYERA;
		}
		else if (foundLayerBPixel && priorityLayerB)
		{
			layerIndex = LAYERINDEX_LAYERB;
		}
		else if (foundSpritePixel)
		{
			layerIndex = LAYERINDEX_SPRITE;
		}
		else if (foundLayerAPixel)
		{
			layerIndex = LAYERINDEX_LAYERA;
		}
		else if (foundLayerBPixel)
		{
			layerIndex = LAYERINDEX_LAYERB;
		}
		else
		{
			layerIndex = LAYERINDEX_BACKGROUND;
		}
	}
	else
	{
		// Perform shadow/highlight mode layer priority calculations. Note that some
		// illustrations in the official documentation from Sega demonstrating the
		// behaviour of shadow/highlight mode are incorrect. In particular, the third and
		// fifth illustrations on page 64 of the "Genesis Software Manual", showing layers
		// B and A being shadowed when a shadow sprite operator is at a lower priority, are
		// incorrect. If any layer is above an operator sprite pixel, the sprite operator
		// is ignored, and the higher priority pixel is output without the sprite operator
		// being applied. This has been confirmed through hardware tests. All other
		// illustrations describing the operation of shadow/highlight mode in relation to
		// layer priority settings appear to be correct.
		if (foundSpritePixel && prioritySprite && !spriteIsShadowOperator && !spriteIsHighlightOperator)
		{
			layerIndex = LAYERINDEX_SPRITE;
		}
		else if (foundLayerAPixel && priorityLayerA)
		{
			layerIndex = LAYERINDEX_LAYERA;
			if (prioritySprite && spriteIsShadowOperator)
			{
				shadow = true;
			}
			else if (prioritySprite && spriteIsHighlightOperator)
			{
				highlight = true;
			}
		}
		else if (foundLayerBPixel && priorityLayerB)
		{
			layerIndex = LAYERINDEX_LAYERB;
			if (prioritySprite && spriteIsShadowOperator)
			{
				shadow = true;
			}
			else if (prioritySprite && spriteIsHighlightOperator)
			{
				highlight = true;
			}
		}
		else if (foundSpritePixel && !spriteIsShadowOperator && !spriteIsHighlightOperator)
		{
			layerIndex = LAYERINDEX_SPRITE;
			if (!priorityLayerA && !priorityLayerB)
			{
				shadow = true;
			}
		}
		else if (foundLayerAPixel)
		{
			layerIndex = LAYERINDEX_LAYERA;
			if (!priorityLayerA && !priorityLayerB)
			{
				shadow = true;
			}
			if (spriteIsShadowOperator)
			{
				shadow = true;
			}
			else if (spriteIsHighlightOperator)
			{
				highlight = true;
			}
		}
		else if (foundLayerBPixel)
		{
			layerIndex = LAYERINDEX_LAYERB;
			if (!priorityLayerA && !priorityLayerB)
			{
				shadow = true;
			}
			if (spriteIsShadowOperator)
			{
				shadow = true;
			}
			else if (spriteIsHighlightOperator)
			{
				highlight = true;
			}
		}
		else
		{
			layerIndex = LAYERINDEX_BACKGROUND;
			if (!priorityLayerA && !priorityLayerB)
			{
				shadow = true;
			}
			if (spriteIsShadowOperator)
			{
				shadow = true;
			}
			else if (spriteIsHighlightOperator)
			{
				highlight = true;
			}
		}

		// If shadow and highlight are both set, they cancel each other out. This is why a
		// sprite acting as a highlight operator is unable to highlight layer A, B, or the
		// background, if layers A and B both have their priority bits unset. This has been
		// confirmed on the hardware.
		if (shadow && highlight)
		{
			shadow = false;
			highlight = false;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::CalculatePatternDataRowNumber(unsigned int patternRowNumberNoFlip, bool interlaceMode2Active, const Data& mappingData) const
{
	// Calculate the final number of the pattern row to read, taking into account vertical
	// flip if it is specified in the block mapping.
	// Mapping (Pattern Name) data format:
	// -----------------------------------------------------------------
	// |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// |---------------------------------------------------------------|
	// |Pri|PalRow |VF |HF |              Pattern Number               |
	// -----------------------------------------------------------------
	// Pri:    Priority Bit
	// PalRow: The palette row number to use when displaying the pattern data
	// VF:     Vertical Flip
	// HF:     Horizontal Flip
	const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
	unsigned int patternRowNumber = mappingData.GetBit(12)? (rowsPerTile - 1) - patternRowNumberNoFlip: patternRowNumberNoFlip;
	return patternRowNumber;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned int S315_5313::CalculatePatternDataRowAddress(unsigned int patternRowNumber, unsigned int patternCellOffset, bool interlaceMode2Active, const Data& mappingData) const
{
	// The address of the pattern data to read is determined by combining the number of the
	// pattern (tile) with the row of the pattern to be read. The way the data is combined
	// is different under interlace mode 2, where patterns are 16 pixels high instead of
	// the usual 8 pixels. The format for pattern data address decoding is as follows when
	// interlace mode 2 is not active:
	// -----------------------------------------------------------------
	// |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// |---------------------------------------------------------------|
	// |              Pattern Number               |Pattern Row| 0 | 0 |
	// -----------------------------------------------------------------
	// When interlace mode 2 is active, the pattern data address decoding is as follows:
	// -----------------------------------------------------------------
	// |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// |---------------------------------------------------------------|
	// |            Pattern Number             |  Pattern Row  | 0 | 0 |
	// -----------------------------------------------------------------
	// Note that we grab the entire mapping data block as the block number when calculating
	// the address. This is because the resulting address is wrapped to keep it within the
	// VRAM boundaries. Due to this wrapping, in reality only the lower 11 bits of the
	// mapping data are effective when determining the block number, or the lower 10 bits
	// in the case of interlace mode 2.
	//##TODO## Test the above assertion on the TeraDrive with the larger VRAM mode active
	static const unsigned int patternDataRowByteSize = 4;
	const unsigned int rowsPerTile = (!interlaceMode2Active)? 8: 16;
	const unsigned int blockPatternByteSize = rowsPerTile * patternDataRowByteSize;
	unsigned int patternDataAddress = (((mappingData.GetData() + patternCellOffset) * blockPatternByteSize) + (patternRowNumber * patternDataRowByteSize)) % VramSize;
	return patternDataAddress;
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::CalculateEffectiveCellScrollSize(unsigned int hszState, unsigned int vszState, unsigned int& effectiveScrollWidth, unsigned int& effectiveScrollHeight) const
{
	// This method performs a simple version of the logic that's implemented in the
	// DigitalRenderReadMappingDataPair method. The purpose of this function is to allow
	// the effective size of the scroll planes to be easily calculated by debug features.
	unsigned int screenSizeModeH = hszState;
	unsigned int screenSizeModeV = ((vszState & 0x1) & ((~hszState & 0x02) >> 1)) | ((vszState & 0x02) & ((~hszState & 0x01) << 1));
	effectiveScrollWidth = (screenSizeModeH+1) * 32;
	effectiveScrollHeight = (screenSizeModeV+1) * 32;
	if (screenSizeModeH == 2)
	{
		effectiveScrollWidth = 32;
		effectiveScrollHeight = 1;
	}
	else if (screenSizeModeV == 2)
	{
		effectiveScrollWidth = 32;
		effectiveScrollHeight = 32;
	}
}

//----------------------------------------------------------------------------------------------------------------------
S315_5313::DecodedPaletteColorEntry S315_5313::ReadDecodedPaletteColor(unsigned int paletteRow, unsigned int paletteIndex) const
{
	// This method is provided purely for debug windows, and provides an easy and
	// convenient way to decode palette colours. The actual render process uses a different
	// method.

	// Read the raw palette entry from CRAM
	static const unsigned int paletteEntriesPerLine = 16;
	Data paletteEntry(16);
	unsigned char byte1 = _cram->ReadCommitted((paletteIndex + (paletteRow * paletteEntriesPerLine)) * 2);
	unsigned char byte2 = _cram->ReadCommitted(((paletteIndex + (paletteRow * paletteEntriesPerLine)) * 2) + 1);
	paletteEntry.SetUpperHalf(byte1);
	paletteEntry.SetLowerHalf(byte2);

	// Extract the actual RGB colour data from the palette entry, and return it to the
	// caller.
	DecodedPaletteColorEntry color;
	color.r = paletteEntry.GetDataSegment(1, 3);
	color.g = paletteEntry.GetDataSegment(5, 3);
	color.b = paletteEntry.GetDataSegment(9, 3);
	return color;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned char S315_5313::ColorValueTo8BitValue(unsigned int colorValue, bool shadow, bool highlight) const
{
	if (shadow == highlight)
	{
		return PaletteEntryTo8Bit[colorValue];
	}
	else if (shadow && !highlight)
	{
		return PaletteEntryTo8BitShadow[colorValue];
	}
	return PaletteEntryTo8BitHighlight[colorValue];
}

//----------------------------------------------------------------------------------------------------------------------
Marshal::Ret<std::list<S315_5313::SpriteBoundaryLineEntry>> S315_5313::GetSpriteBoundaryLines(unsigned int planeNo) const
{
	std::unique_lock<std::mutex> spriteLock(_spriteBoundaryMutex[planeNo]);
	return _imageBufferSpriteBoundaryLines[planeNo];
}

//----------------------------------------------------------------------------------------------------------------------
// Sprite list debugging functions
//----------------------------------------------------------------------------------------------------------------------
S315_5313::SpriteMappingTableEntry S315_5313::GetSpriteMappingTableEntry(unsigned int spriteTableBaseAddress, unsigned int entryNo) const
{
	// Calculate the address in VRAM of this sprite table entry
	static const unsigned int spriteTableEntrySize = 8;
	unsigned int spriteTableEntryAddress = spriteTableBaseAddress + (entryNo * spriteTableEntrySize);
	spriteTableEntryAddress %= VramSize;

	// Read all raw data for the sprite from the sprite attribute table in VRAM
	SpriteMappingTableEntry entry;
	entry.rawDataWord0 = ((unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+0) << 8) | (unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+1);
	entry.rawDataWord1 = ((unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+2) << 8) | (unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+3);
	entry.rawDataWord2 = ((unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+4) << 8) | (unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+5);
	entry.rawDataWord3 = ((unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+6) << 8) | (unsigned int)_vram->ReadCommitted(spriteTableEntryAddress+7);

	// Decode the sprite mapping data
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word0  |---------------------------------------------------------------|
	//        |                          Vertical Pos                         |
	//        -----------------------------------------------------------------
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word1  |---------------------------------------------------------------|
	//        | /   /   /   / | HSize | VSize | / |         Link Data         |
	//        -----------------------------------------------------------------
	//        HSize:     Horizontal size of the sprite
	//        VSize:     Vertical size of the sprite
	//        Link Data: Next sprite entry to read from table during sprite rendering
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word2  |---------------------------------------------------------------|
	//        |Pri|PalRow |VF |HF |              Pattern Number               |
	//        -----------------------------------------------------------------
	//        Pri:    Priority Bit
	//        PalRow: The palette row number to use when displaying the pattern data
	//        VF:     Vertical Flip
	//        HF:     Horizontal Flip
	//        Mapping (Pattern Name) data format:
	//        -----------------------------------------------------------------
	//        |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	// Word3  |---------------------------------------------------------------|
	//        |                         Horizontal Pos                        |
	//        -----------------------------------------------------------------
	entry.ypos = entry.rawDataWord0.GetData();
	entry.width = entry.rawDataWord1.GetDataSegment(10, 2);
	entry.height = entry.rawDataWord1.GetDataSegment(8, 2);
	entry.link = entry.rawDataWord1.GetDataSegment(0, 7);
	entry.priority = entry.rawDataWord2.GetBit(15);
	entry.paletteLine = entry.rawDataWord2.GetDataSegment(13, 2);
	entry.vflip = entry.rawDataWord2.GetBit(12);
	entry.hflip = entry.rawDataWord2.GetBit(11);
	entry.blockNumber = entry.rawDataWord2.GetDataSegment(0, 11);
	entry.xpos = entry.rawDataWord3.GetData();

	return entry;
}

//----------------------------------------------------------------------------------------------------------------------
void S315_5313::SetSpriteMappingTableEntry(unsigned int spriteTableBaseAddress, unsigned int entryNo, const SpriteMappingTableEntry& entry, bool useSeparatedData)
{
	// Select the data to write back to the sprite table
	Data rawDataWord0(entry.rawDataWord0);
	Data rawDataWord1(entry.rawDataWord1);
	Data rawDataWord2(entry.rawDataWord2);
	Data rawDataWord3(entry.rawDataWord3);
	if (useSeparatedData)
	{
		rawDataWord0 = entry.ypos;
		rawDataWord1.SetDataSegment(10, 2, entry.width);
		rawDataWord1.SetDataSegment(8, 2, entry.height);
		rawDataWord1.SetDataSegment(0, 7, entry.link);
		rawDataWord2.SetBit(15, entry.priority);
		rawDataWord2.SetDataSegment(13, 2, entry.paletteLine);
		rawDataWord2.SetBit(12, entry.vflip);
		rawDataWord2.SetBit(11, entry.hflip);
		rawDataWord2.SetDataSegment(0, 11, entry.blockNumber);
		rawDataWord3 = entry.xpos;
	}

	// Calculate the address in VRAM of this sprite table entry
	static const unsigned int spriteTableEntrySize = 8;
	unsigned int spriteTableEntryAddress = spriteTableBaseAddress + (entryNo * spriteTableEntrySize);
	spriteTableEntryAddress %= VramSize;

	// Write the raw data for the sprite to the sprite attribute table in VRAM
	_vram->WriteLatest(spriteTableEntryAddress+0, (unsigned char)rawDataWord0.GetUpperHalf());
	_vram->WriteLatest(spriteTableEntryAddress+1, (unsigned char)rawDataWord0.GetLowerHalf());
	_vram->WriteLatest(spriteTableEntryAddress+2, (unsigned char)rawDataWord1.GetUpperHalf());
	_vram->WriteLatest(spriteTableEntryAddress+3, (unsigned char)rawDataWord1.GetLowerHalf());
	_vram->WriteLatest(spriteTableEntryAddress+4, (unsigned char)rawDataWord2.GetUpperHalf());
	_vram->WriteLatest(spriteTableEntryAddress+5, (unsigned char)rawDataWord2.GetLowerHalf());
	_vram->WriteLatest(spriteTableEntryAddress+6, (unsigned char)rawDataWord3.GetUpperHalf());
	_vram->WriteLatest(spriteTableEntryAddress+7, (unsigned char)rawDataWord3.GetLowerHalf());

	// Calculate the address in the internal sprite cache of the cached portion of this
	// sprite entry
	static const unsigned int spriteCacheEntrySize = 4;
	unsigned int spriteCacheTableEntryAddress = spriteTableBaseAddress + (entryNo * spriteCacheEntrySize);
	spriteCacheTableEntryAddress %= SpriteCacheSize;

	// Update the sprite cache to make it contain the new data
	_spriteCache->WriteLatest(spriteCacheTableEntryAddress+0, (unsigned char)rawDataWord0.GetUpperHalf());
	_spriteCache->WriteLatest(spriteCacheTableEntryAddress+1, (unsigned char)rawDataWord0.GetLowerHalf());
	_spriteCache->WriteLatest(spriteCacheTableEntryAddress+2, (unsigned char)rawDataWord1.GetUpperHalf());
	_spriteCache->WriteLatest(spriteCacheTableEntryAddress+3, (unsigned char)rawDataWord1.GetLowerHalf());
}
