import React, { useEffect, useRef } from 'react';
import L from 'leaflet';

interface MapViewProps {
  coordinates?: [number, number][]; // [lat, lon]
  sourceNode?: number;
  destNode?: number;
  sourceCoords?: [number, number];
  destCoords?: [number, number];
}

export const MapView: React.FC<MapViewProps> = ({
  coordinates,
  sourceNode,
  destNode,
}) => {
  const mapContainerRef = useRef<HTMLDivElement>(null);
  const mapInstanceRef = useRef<L.Map | null>(null);
  const polylineLayerRef = useRef<L.Polyline | null>(null);
  const markersLayerRef = useRef<L.LayerGroup | null>(null);

  useEffect(() => {
    if (!mapContainerRef.current) return;

    if (!mapInstanceRef.current) {
      // Default center: San Francisco Bay Area
      const map = L.map(mapContainerRef.current, {
        center: [37.7749, -122.4194],
        zoom: 9,
        zoomControl: true,
      });

      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
        maxZoom: 18,
      }).addTo(map);

      mapInstanceRef.current = map;
      markersLayerRef.current = L.layerGroup().addTo(map);
    }

    const map = mapInstanceRef.current;
    const markersGroup = markersLayerRef.current;

    if (markersGroup) markersGroup.clearLayers();
    if (polylineLayerRef.current) {
      polylineLayerRef.current.remove();
      polylineLayerRef.current = null;
    }

    if (coordinates && coordinates.length > 0 && map && markersGroup) {
      const latLngs: L.LatLngExpression[] = coordinates.map((c) => [c[0], c[1]]);

      // Draw polyline
      const polyline = L.polyline(latLngs, {
        color: '#2563eb', // Indigo / Blue 600
        weight: 5,
        opacity: 0.85,
        lineJoin: 'round',
      }).addTo(map);
      polylineLayerRef.current = polyline;

      // Start Marker
      const startCoord = coordinates[0];
      const startMarker = L.circleMarker([startCoord[0], startCoord[1]], {
        radius: 8,
        fillColor: '#10b981', // Emerald 500
        color: '#ffffff',
        weight: 2,
        fillOpacity: 1,
      }).addTo(markersGroup);
      startMarker.bindPopup(`<b>Origin Node ${sourceNode || 1}</b><br/>Lat: ${startCoord[0].toFixed(5)}, Lon: ${startCoord[1].toFixed(5)}`);

      // Destination Marker
      const endCoord = coordinates[coordinates.length - 1];
      const endMarker = L.circleMarker([endCoord[0], endCoord[1]], {
        radius: 8,
        fillColor: '#ef4444', // Red 500
        color: '#ffffff',
        weight: 2,
        fillOpacity: 1,
      }).addTo(markersGroup);
      endMarker.bindPopup(`<b>Destination Node ${destNode || ''}</b><br/>Lat: ${endCoord[0].toFixed(5)}, Lon: ${endCoord[1].toFixed(5)}`);

      // Fit map view to route bounds
      try {
        map.fitBounds(polyline.getBounds(), {
          padding: [30, 30],
          maxZoom: 14,
        });
      } catch (e) {
        console.warn('Error adjusting bounds:', e);
      }
    }

    return () => {
      // Don't destroy map on every coordinate update, only unmount
    };
  }, [coordinates, sourceNode, destNode]);

  useEffect(() => {
    return () => {
      if (mapInstanceRef.current) {
        mapInstanceRef.current.remove();
        mapInstanceRef.current = null;
      }
    };
  }, []);

  return (
    <div className="relative w-full h-full min-h-[420px] rounded-xl overflow-hidden shadow-inner border border-slate-700/50">
      <div ref={mapContainerRef} className="w-full h-full min-h-[420px] z-0" />
      {(!coordinates || coordinates.length === 0) && (
        <div className="absolute inset-0 bg-slate-900/40 backdrop-blur-[2px] flex items-center justify-center pointer-events-none z-10">
          <div className="bg-slate-900/90 border border-slate-700 text-slate-200 px-4 py-3 rounded-lg shadow-xl text-center max-w-sm">
            <p className="font-semibold text-sm">San Francisco Bay Area Road Network</p>
            <p className="text-xs text-slate-400 mt-1">Select an origin and destination node to compute and render the shortest path geometry.</p>
          </div>
        </div>
      )}
    </div>
  );
};
