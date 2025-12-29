
// Copyright 2010 Palm Inc.

#include "config.h"
#include "DocumentMarkerController.h"

#include "Node.h"
#include "Range.h"
#include "VisiblePosition.h"
#include "visible_units.h"
#include "TextIterator.h"

namespace WebCore {

static IntRect placeholderRectForMarker()
{
    return IntRect(-1, -1, -1, -1);
}

DocumentMarkerController::DocumentMarkerController()
    : m_updatingMarkersEnabled(true)
{
}

void DocumentMarkerController::addMarker(Node* node, DocumentMarker newMarker)
{
    ASSERT(newMarker.endOffset >= newMarker.startOffset);
    if (newMarker.endOffset == newMarker.startOffset)
        return;

    MarkerMapVectorPair* vectorPair = m_markers.get(node);

    if (!vectorPair) {
        vectorPair = new MarkerMapVectorPair;
        vectorPair->first.append(newMarker);
        vectorPair->second.append(placeholderRectForMarker());
        m_markers.set(node, vectorPair);
    } else {
        Vector<DocumentMarker>& markers = vectorPair->first;
        Vector<IntRect>& rects = vectorPair->second;
        size_t numMarkers = markers.size();
        ASSERT(numMarkers == rects.size());
        size_t i;
        // Iterate over all markers whose start offset is less than or equal to the new marker's.
        // If one of them is of the same type as the new marker and touches it or intersects with it
        // (there is at most one), remove it and adjust the new marker's start offset to encompass it.
        for (i = 0; i < numMarkers;) {
            DocumentMarker marker = markers[i];
            if (marker.startOffset > newMarker.startOffset)
                break;
            if (marker.type == newMarker.type && marker.endOffset >= newMarker.startOffset) {
                newMarker.startOffset = marker.startOffset;
                markers.remove(i);
                rects.remove(i);
                numMarkers--;
                continue;
            }
            // spelling & replacement markers should delete overlapping "spell pending" markers
            if (marker.type == DocumentMarker::SpellPending &&
                DocumentMarker::isReplacementForSpellPending(newMarker.type) &&
                marker.endOffset >= newMarker.startOffset) {
                markers.remove(i);
                rects.remove(i);
                numMarkers--;
                continue;
            }
            ++i;
        }
        size_t j = i;
        // Iterate over all markers whose end offset is less than or equal to the new marker's,
        // removing markers of the same type as the new marker which touch it or intersect with it,
        // adjusting the new marker's end offset to cover them if necessary.
        while (j < numMarkers) {
            DocumentMarker marker = markers[j];
            if (marker.startOffset > newMarker.endOffset)
                break;
            if (marker.type == newMarker.type) {
                markers.remove(j);
                rects.remove(j);
                numMarkers--;
                if (newMarker.endOffset <= marker.endOffset) {
                    newMarker.endOffset = marker.endOffset;
                    continue;
                }
            }
            // spelling & replacement markers should delete overlapping "spell pending" markers
            if (marker.type == DocumentMarker::SpellPending &&
                DocumentMarker::isReplacementForSpellPending(newMarker.type)) {
                markers.remove(j);
                rects.remove(j);
                numMarkers--;
                continue;
            }
            ++j;
        }
        // At this point i points to the node before which we want to insert.
        markers.insert(i, newMarker);
        rects.insert(i, placeholderRectForMarker());
    }

    // repaint the affected node
    if (node->renderer() && !DocumentMarker::isHiddenType(newMarker.type))
        node->renderer()->repaint();
}

void DocumentMarkerController::removeMarkers(Node* node, unsigned startOffset, int length, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;

    if (!m_updatingMarkersEnabled)
        return;

    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    Vector<IntRect>& rects = vectorPair->second;
    ASSERT(markers.size() == rects.size());
    bool docDirty = false;
    unsigned endOffset = startOffset + length;
    for (size_t i = 0; i < markers.size();) {
        DocumentMarker marker = markers[i];

        // markers are returned in order, so stop if we are now past the specified range
        // Marker who's startOffset == range endOffset needs to be checked
        // separately because it is on the boundary.
        if (marker.startOffset > endOffset)
            break;

        // skip marker that is wrong type or before target
        if (marker.endOffset < startOffset || (marker.type != markerType && markerType != DocumentMarker::AllMarkers)) {
            i++;
            continue;
        }

        // check the end boundary: see if the original word changed
        // For example if the text was "sunny day" and space between
        // words "sunny" and "day" got deleted giving text "sunnyday"
        // marker that belonged to the word "day" should be deleted
        if (marker.startOffset == endOffset) {
            VisiblePosition oldWordStart(node, startOffset, UPSTREAM);
            VisiblePosition newWordStart = startOfWord(oldWordStart, RightWordIfOnBoundary);
            // if the word start is not at the same offset as before, delete the marker
            if (oldWordStart == newWordStart) {
                i++;
                continue;
            }
        }

        // check the start boundary
        // For example if the text was "sunny day" and space between
        // words "sunny" and "day" got deleted giving text "sunnyday"
        // marker that belonged to the word "sunny" should be deleted
        if (marker.endOffset == startOffset) {
            VisiblePosition oldWordEnd(node, startOffset, UPSTREAM);
            VisiblePosition newWordEnd = endOfWord(oldWordEnd, LeftWordIfOnBoundary);
            // if the word start is not at the same ofset as before, delete the marker
            if (oldWordEnd == newWordEnd) {
                i++;
                continue;
            }
        }

        // at this point we know that marker and target intersect in some way
        docDirty = true;

        // pitch the old marker and any associated rect
        markers.remove(i);
        rects.remove(i);

        // We never want to patch up pieces of old markers.
        // When text changes we want to re-evaluate the words that changed.
        // That is why that piece of code was removed.
    }

    if (markers.isEmpty()) {
        ASSERT(rects.isEmpty());
        m_markers.remove(node);
        delete vectorPair;
    }

    // Repaint the affected node unless we deleted only hidden markers
    if (docDirty && node->renderer() && !DocumentMarker::isHiddenType(markerType))
        node->renderer()->repaint();
}


// Returns the range of text for marker of type SpellPending with specified description.
// It will search the given range of text (lookupRange) for a first marker with given description
// and return the text range for the found marker.
PassRefPtr<Range> DocumentMarkerController::findRangeForPendingSpellMarker(const Range* lookupRange, const String& description)
{
    if (!lookupRange)
        return 0;

    if (description.isEmpty())
        return 0;

    if (m_markers.isEmpty())
        return 0;

    ExceptionCode ec = 0;
    Node* startContainer = lookupRange->startContainer(ec);
    Node* endContainer = lookupRange->endContainer(ec);

    Node* pastLastNode = lookupRange->pastLastNode();
    for (Node* node = lookupRange->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        MarkerMapVectorPair* vectorPair = m_markers.get(node);
        if (vectorPair) {
            Vector<DocumentMarker>& nodeMarkers = vectorPair->first;
            for (size_t i = 0; i < nodeMarkers.size(); ++i) {
                DocumentMarker marker = nodeMarkers[i];
                if (marker.type == DocumentMarker::SpellPending && marker.description == description)
                    return Range::create( node->document(), node, marker.startOffset, node, marker.endOffset);
            }
        }
    }

    return 0;
}

PassRefPtr<Range> DocumentMarkerController::findRangeForWordCompletionMarker(Node *node)
{
	if (!node)
		return 0;

	MarkerMapVectorPair* vectorPair = m_markers.get(node);
	if (vectorPair) {
		Vector<DocumentMarker>& nodeMarkers = vectorPair->first;
		for (size_t i = 0; i < nodeMarkers.size(); ++i) {
			DocumentMarker& marker = nodeMarkers[i];
			if (marker.type == DocumentMarker::WordCompletion)
				return Range::create( node->document(), node, marker.startOffset, node, marker.endOffset);
		}
	}

    return 0;
}

DocumentMarker* DocumentMarkerController::markerContainedInRange(const Range *range, DocumentMarker::MarkerType markerType)
{
    if (!range)
        return 0;

    if (m_markers.isEmpty())
        return 0;

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = node->traverseNextNode()) {
        MarkerMapVectorPair* vectorPair = m_markers.get(node);
        if (vectorPair) {
            Vector<DocumentMarker>& nodeMarkers = vectorPair->first;
            for (size_t i = 0; i < nodeMarkers.size(); ++i) {
                DocumentMarker& marker = nodeMarkers[i];

                if (marker.type != markerType && markerType != DocumentMarker::AllMarkers)
                    continue;

                if ((int)marker.startOffset == range->startOffset() && (int)marker.endOffset == range->endOffset())
                    return &marker;
            }
        }
    }

    return 0;
}

// called when text is inserted to check the markers that begin/end in the inserted range.
// if original marked text changed due to insertion, markers will be deleted
// for example let's say that text was "happy small hippo"
// which after the instertion changed to "happiest small hippo".
// the marker which used to be on word "happy" will get deleted, while marker
// belonging to word "small" will stay unchanged.
void DocumentMarkerController::deleteMarkersOnTextInsert(Node *node, unsigned startOffset, int length, DocumentMarker::MarkerType markerType)
{
    if (length <= 0)
        return;

    if (!m_updatingMarkersEnabled)
        return;

    MarkerMapVectorPair* vectorPair = m_markers.get(node);
    if (!vectorPair)
        return;

    Vector<DocumentMarker>& markers = vectorPair->first;
    Vector<IntRect>& rects = vectorPair->second;
    ASSERT(markers.size() == rects.size());
    bool docDirty = false;
    unsigned endOffset = startOffset + length;

    for (size_t i = 0; i < markers.size();) {
        DocumentMarker marker = markers[i];

        if (marker.startOffset > endOffset)
            break;

        if (marker.type == markerType || markerType == DocumentMarker::AllMarkers) {
            // delete markers that were "cut in half" by inserted text
            if (marker.startOffset < startOffset && marker.endOffset > startOffset) {
                docDirty = true;
                markers.remove(i);
                rects.remove(i);
                continue;
            }

            // check the marker ending at the insertion point
            if (marker.endOffset == startOffset) {
                VisiblePosition oldWordEnd(node, marker.endOffset, UPSTREAM);
                VisiblePosition newWordEnd = endOfWord(oldWordEnd, LeftWordIfOnBoundary);
                // if the word start is not at the same ofset as before, delete the marker
                if (oldWordEnd != newWordEnd) {
                    docDirty = true;
                    markers.remove(i);
                    rects.remove(i);
                    continue;
                }
            }
            // check the marker starting at the end of insertion range
            if (marker.startOffset == endOffset) {
                VisiblePosition oldWordStart(node, marker.startOffset, UPSTREAM);
                VisiblePosition newWordStart = startOfWord(oldWordStart, RightWordIfOnBoundary);
                // if the word start is not at the same ofset as before, delete the marker
                if (oldWordStart != newWordStart ) {
                    docDirty = true;
                    markers.remove(i);
                    rects.remove(i);
                    continue;
                }
            }
        }

        ++i;
    }

    if (markers.isEmpty()) {
        ASSERT(rects.isEmpty());
        m_markers.remove(node);
        delete vectorPair;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->repaint();
}

} // WebCore

