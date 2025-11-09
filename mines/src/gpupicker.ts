/**
 * Fast GPU picker that handles dynamic scenes and objects for Three.JS
 *
 * @author bzztbomb https://github.com/bzztbomb
 * @author jfaust https://github.com/jfaust
 *
 * Developed at Torch3D (https://torch.app), thanks for allowing me to release this
 * nice little library!
 *
 */

import {UniformsUtils, UniformsLib, Vector2} from 'three';

var THREE;

var GPUPicker = function(three, renderer, scene, camera) {
  THREE = three;
  // This is the 1x1 pixel render target we use to do the picking
  var pickingTarget = new THREE.WebGLRenderTarget(1, 1, {
    minFilter: THREE.NearestFilter,
    magFilter: THREE.NearestFilter,
    format: THREE.RGBAFormat,
    encoding: THREE.LinearEncoding
  });
  // We need to be inside of .render in order to call renderBufferDirect in renderList() so create an empty scene
  // and use the onAfterRender callback to actually render geometry for picking.
  var emptyScene = new THREE.Scene();
  emptyScene.onAfterRender = renderList;
  // RGBA is 4 channels.
  var pixelBuffer = new Uint8Array(4 * pickingTarget.width * pickingTarget.height);
  var clearColor = new THREE.Color(0xffffff);
  var materialCache = new Map();

  var currClearColor = new THREE.Color();

  this.pick = function(x, y) {
    var w = renderer.domElement.width;
    var h = renderer.domElement.height;
    // Set the projection matrix to only look at the pixel we are interested in.
    camera.setViewOffset(w, h, x, y, 1, 1);

    var currRenderTarget = renderer.getRenderTarget();
    var currAlpha = renderer.getClearAlpha();
    renderer.getClearColor(currClearColor);
    renderer.setRenderTarget(pickingTarget);
    renderer.setClearColor(clearColor);
    renderer.clear();
    renderer.render(emptyScene, camera);
    renderer.readRenderTargetPixels(pickingTarget, 0, 0, pickingTarget.width, pickingTarget.height, pixelBuffer);
    renderer.setRenderTarget(currRenderTarget);
    renderer.setClearColor(currClearColor, currAlpha);
    camera.clearViewOffset();

    var val = (pixelBuffer[0] << 24) + (pixelBuffer[1] << 16) + (pixelBuffer[2] << 8) + pixelBuffer[3];
    return val;
  }
  
// @ts-ignore
UniformsLib.lineId = {
  worldUnits: { value: 1 },
  linewidth: { value: 1 },
  resolution: { value: new Vector2( 1, 1 ) },
  dashOffset: { value: 0 },
  dashScale: { value: 1 },
  dashSize: { value: 1 },
  gapSize: { value: 1 }, // todo FIX - maybe change to totalSize
  objectId: {value: -1}
};

const LineIdShader = {
  uniforms: UniformsUtils.merge( [
    UniformsLib.common,
    // @ts-ignore
    UniformsLib.lineId
  ] ),

 	  vertexShader:
  /* glsl */`
    #include <common>
    #include <logdepthbuf_pars_vertex>
    #include <clipping_planes_pars_vertex>

    uniform float linewidth;
    uniform vec2 resolution;

    attribute vec3 instanceStart;
    attribute vec3 instanceEnd;

    attribute vec3 instanceColorStart;
    attribute vec3 instanceColorEnd;

    #ifdef WORLD_UNITS

      varying vec4 worldPos;
      varying vec3 worldStart;
      varying vec3 worldEnd;

      #ifdef USE_DASH

        varying vec2 vUv;

      #endif

    #else

      varying vec2 vUv;

    #endif

    #ifdef USE_DASH

      uniform float dashScale;
      attribute float instanceDistanceStart;
      attribute float instanceDistanceEnd;
      varying float vLineDistance;

    #endif

    void trimSegment( const in vec4 start, inout vec4 end ) {

      // trim end segment so it terminates between the camera plane and the near plane

      // conservative estimate of the near plane
      float a = projectionMatrix[ 2 ][ 2 ]; // 3nd entry in 3th column
      float b = projectionMatrix[ 3 ][ 2 ]; // 3nd entry in 4th column
      float nearEstimate = - 0.5 * b / a;

      float alpha = ( nearEstimate - start.z ) / ( end.z - start.z );

      end.xyz = mix( start.xyz, end.xyz, alpha );

    }

    void main() {

      #ifdef USE_DASH

        vLineDistance = ( position.y < 0.5 ) ? dashScale * instanceDistanceStart : dashScale * instanceDistanceEnd;
        vUv = uv;

      #endif

      float aspect = resolution.x / resolution.y;

      // camera space
      vec4 start = modelViewMatrix * vec4( instanceStart, 1.0 );
      vec4 end = modelViewMatrix * vec4( instanceEnd, 1.0 );

      #ifdef WORLD_UNITS

        worldStart = start.xyz;
        worldEnd = end.xyz;

      #else

        vUv = uv;

      #endif

      // special case for perspective projection, and segments that terminate either in, or behind, the camera plane
      // clearly the gpu firmware has a way of addressing this issue when projecting into ndc space
      // but we need to perform ndc-space calculations in the shader, so we must address this issue directly
      // perhaps there is a more elegant solution -- WestLangley

      bool perspective = ( projectionMatrix[ 2 ][ 3 ] == - 1.0 ); // 4th entry in the 3rd column

      if ( perspective ) {

        if ( start.z < 0.0 && end.z >= 0.0 ) {

          trimSegment( start, end );

        } else if ( end.z < 0.0 && start.z >= 0.0 ) {

          trimSegment( end, start );

        }

      }

      // clip space
      vec4 clipStart = projectionMatrix * start;
      vec4 clipEnd = projectionMatrix * end;

      // ndc space
      vec3 ndcStart = clipStart.xyz / clipStart.w;
      vec3 ndcEnd = clipEnd.xyz / clipEnd.w;

      // direction
      vec2 dir = ndcEnd.xy - ndcStart.xy;

      // account for clip-space aspect ratio
      dir.x *= aspect;
      dir = normalize( dir );

      #ifdef WORLD_UNITS

        vec3 worldDir = normalize( end.xyz - start.xyz );
        vec3 tmpFwd = normalize( mix( start.xyz, end.xyz, 0.5 ) );
        vec3 worldUp = normalize( cross( worldDir, tmpFwd ) );
        vec3 worldFwd = cross( worldDir, worldUp );
        worldPos = position.y < 0.5 ? start: end;

        // height offset
        float hw = linewidth * 0.5;
        worldPos.xyz += position.x < 0.0 ? hw * worldUp : - hw * worldUp;

        // don't extend the line if we're rendering dashes because we
        // won't be rendering the endcaps
        #ifndef USE_DASH

          // cap extension
          worldPos.xyz += position.y < 0.5 ? - hw * worldDir : hw * worldDir;

          // add width to the box
          worldPos.xyz += worldFwd * hw;

          // endcaps
          if ( position.y > 1.0 || position.y < 0.0 ) {

            worldPos.xyz -= worldFwd * 2.0 * hw;

          }

        #endif

        // project the worldpos
        vec4 clip = projectionMatrix * worldPos;

        // shift the depth of the projected points so the line
        // segments overlap neatly
        vec3 clipPose = ( position.y < 0.5 ) ? ndcStart : ndcEnd;
        clip.z = clipPose.z * clip.w;

      #else

        vec2 offset = vec2( dir.y, - dir.x );
        // undo aspect ratio adjustment
        dir.x /= aspect;
        offset.x /= aspect;

        // sign flip
        if ( position.x < 0.0 ) offset *= - 1.0;

        // endcaps
        if ( position.y < 0.0 ) {

          offset += - dir;

        } else if ( position.y > 1.0 ) {

          offset += dir;

        }

        // adjust for linewidth
        offset *= linewidth;

        // adjust for clip-space to screen-space conversion // maybe resolution should be based on viewport ...
        offset /= resolution.y;

        // select end
        vec4 clip = ( position.y < 0.5 ) ? clipStart : clipEnd;

        // back to clip space
        offset *= clip.w;

        clip.xy += offset;

      #endif

      gl_Position = clip;

      vec4 mvPosition = ( position.y < 0.5 ) ? start : end; // this is an approximation

      #include <logdepthbuf_vertex>
      #include <clipping_planes_vertex>

    }
    `,

  fragmentShader:
  /* glsl */`
    // for gpu picking
    uniform vec4 objectId;
    uniform float linewidth;

    #ifdef USE_DASH

      uniform float dashOffset;
      uniform float dashSize;
      uniform float gapSize;

    #endif

    varying float vLineDistance;

    #ifdef WORLD_UNITS

      varying vec4 worldPos;
      varying vec3 worldStart;
      varying vec3 worldEnd;

      #ifdef USE_DASH

        varying vec2 vUv;

      #endif

    #else

      varying vec2 vUv;

    #endif

    #include <common>
    #include <logdepthbuf_pars_fragment>
    #include <clipping_planes_pars_fragment>

    vec2 closestLineToLine(vec3 p1, vec3 p2, vec3 p3, vec3 p4) {

      float mua;
      float mub;

      vec3 p13 = p1 - p3;
      vec3 p43 = p4 - p3;

      vec3 p21 = p2 - p1;

      float d1343 = dot( p13, p43 );
      float d4321 = dot( p43, p21 );
      float d1321 = dot( p13, p21 );
      float d4343 = dot( p43, p43 );
      float d2121 = dot( p21, p21 );

      float denom = d2121 * d4343 - d4321 * d4321;

      float numer = d1343 * d4321 - d1321 * d4343;

      mua = numer / denom;
      mua = clamp( mua, 0.0, 1.0 );
      mub = ( d1343 + d4321 * ( mua ) ) / d4343;
      mub = clamp( mub, 0.0, 1.0 );

      return vec2( mua, mub );

    }

    void main() {

      #include <clipping_planes_fragment>

      #ifdef USE_DASH

        if ( vUv.y < - 1.0 || vUv.y > 1.0 ) discard; // discard endcaps

        if ( mod( vLineDistance + dashOffset, dashSize + gapSize ) > dashSize ) discard; // todo - FIX

      #endif

      #ifdef WORLD_UNITS

        // Find the closest points on the view ray and the line segment
        vec3 rayEnd = normalize( worldPos.xyz ) * 1e5;
        vec3 lineDir = worldEnd - worldStart;
        vec2 params = closestLineToLine( worldStart, worldEnd, vec3( 0.0, 0.0, 0.0 ), rayEnd );

        vec3 p1 = worldStart + lineDir * params.x;
        vec3 p2 = rayEnd * params.y;
        vec3 delta = p1 - p2;
        float len = length( delta );
        float norm = len / linewidth;

        #ifndef USE_DASH

          #ifdef USE_ALPHA_TO_COVERAGE

            float dnorm = fwidth( norm );
            alpha = 1.0 - smoothstep( 0.5 - dnorm, 0.5 + dnorm, norm );

          #else

            if ( norm > 0.5 ) {

              discard;

            }

          #endif

        #endif

      #else

        #ifdef USE_ALPHA_TO_COVERAGE

          // artifacts appear on some hardware if a derivative is taken within a conditional
          float a = vUv.x;
          float b = ( vUv.y > 0.0 ) ? vUv.y - 1.0 : vUv.y + 1.0;
          float len2 = a * a + b * b;
          float dlen = fwidth( len2 );

          if ( abs( vUv.y ) > 1.0 ) {

            alpha = 1.0 - smoothstep( 1.0 - dlen, 1.0 + dlen, len2 );

          }

        #else

          if ( abs( vUv.y ) > 1.0 ) {

            float a = vUv.x;
            float b = ( vUv.y > 0.0 ) ? vUv.y - 1.0 : vUv.y + 1.0;
            float len2 = a * a + b * b;

            if ( len2 > 1.0 ) discard;

          }

        #endif

      #endif

      #include <logdepthbuf_fragment>

      gl_FragColor = objectId;

    }
    `
};

  function renderList() {
    // This is the magic, these render lists are still filled with valid data.  So we can
    // submit them again for picking and save lots of work!
    var renderList = renderer.renderLists.get(scene, 0);
    renderList.opaque.forEach(processItem);
    renderList.transmissive.forEach(processItem);
    renderList.transparent.forEach(processItem);
  }

  function processItem(renderItem) {
    var object = renderItem.object;
    var objId = object.id;
    var geometry = renderItem.geometry;
    var renderMaterial;
    if (renderItem.material.isLineMaterial) {
      renderMaterial = renderItem.object.pickingMaterial ? renderItem.object.pickingMaterial : materialCache.get('line');
      if (!renderMaterial) {
        renderMaterial = new THREE.ShaderMaterial({
          type: 'LineMaterial',
          uniforms: UniformsUtils.clone(LineIdShader.uniforms),
          vertexShader: LineIdShader.vertexShader,
          fragmentShader: LineIdShader.fragmentShader,
          clipping: true // required for clipping support,
        });
        materialCache.set('line',renderMaterial);
      }
      renderMaterial.defines = renderItem.material.defines;
      //copy line settings.
      Object.assign(renderMaterial.uniforms, renderItem.material.uniforms);
    } else if(renderItem.material.isLineBasicMaterial){
      return
    }
    else{
      renderMaterial = renderItem.object.pickingMaterial ? renderItem.object.pickingMaterial : materialCache.get('mesh');
      if (!renderMaterial) {
        let vertexShader = THREE.ShaderChunk.meshbasic_vert;
        renderMaterial = new THREE.ShaderMaterial({
          vertexShader: vertexShader,
          fragmentShader: `
          uniform vec4 objectId;
          void main() {
            gl_FragColor = objectId;
          }
        `,
          side: THREE.DoubleSide,
        });
        materialCache.set('mesh', renderMaterial);
        renderMaterial.uniforms = {
          objectId: { value: [1.0, 1.0, 1.0, 1.0] },
        };
      }
    }
    if(renderMaterial){
      renderMaterial.uniforms.objectId.value = [
        ( objId >> 24 & 255 ) / 255,
        ( objId >> 16 & 255 ) / 255,
        ( objId >> 8 & 255 ) / 255,
        ( objId & 255 ) / 255,
      ];
      renderMaterial.uniformsNeedUpdate = true;
      renderer.renderBufferDirect(camera, null, geometry, renderMaterial, object, null);
    }
  }
}

export { GPUPicker };
