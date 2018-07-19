int cdelta( inout int a, inout int b ){
#ifdef USE_MORTON_32
    uint acode = Mortoncodes[a], bcode = Mortoncodes[b];
    int pfx = nlz(acode^bcode);
    return pfx + (pfx < 32 ? 0 : nlz(a^b));
#else
    uvec2 acode = Mortoncodes[a], bcode = Mortoncodes[b];
    int pfx = nlz(acode^bcode);
    return pfx + (pfx < 64 ? 0 : nlz(a^b));
#endif
}

int findSplit( inout int left, inout int right ) {
    int split = left, nstep = right - left, nsplit = split + nstep;
    int commonPrefix = cdelta(split, nsplit);
    [[flatten]]
    if (commonPrefix >= 64 || nstep <= 1) { // if morton code equals or so small range
        split = (split + nsplit)>>1;
    } else 
    { //fast search SAH split
        [[dependency_infinite]]
        do {
            nstep = (nstep + 1) >> 1, nsplit = split + nstep;
            if (cdelta(split, nsplit) > commonPrefix) { split = nsplit; }
        } while (nstep > 1);
    }
    return clamp(split, left, right-1);
}

void splitNode(in int fID, in int side) {
    // select elements, include sibling
    int prID = fID + side;

    // splitting nodes
    ivec4 _pdata = imageLoad(bvhMeta, prID)-1;
    //Flags[prID] = 0; // reset flag of refit

    [[flatten]]
    if (_pdata.x >= 0 && _pdata.y >= 0) {

        [[flatten]]
        if (_pdata.y != _pdata.x) {

            // find split
            int split = findSplit(_pdata.x, _pdata.y);
            ivec4 transplit = ivec4(_pdata.x, split+0, split+1, _pdata.y);
            bvec2 isLeaf = lessThan(transplit.yw - transplit.xz, ivec2(1,1));
            
            // resolve branch
            int hd = (split+1) << 1;
            imageStore(bvhMeta, prID, ivec4(hd.xx+ivec2(1,2), _pdata.zw+1));
            imageStore(bvhMeta, hd+0, ivec4(transplit.xy, prID, _pdata.w)+1);
            imageStore(bvhMeta, hd+1, ivec4(transplit.zw, prID, _pdata.w)+1);
            Actives[swapOut+wID(aCounterInc())] = hd+1;
        } else 
        { LeafIndices[cCounterInc()] = prID+1; }
    }
}
