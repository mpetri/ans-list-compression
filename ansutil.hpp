#pragma once

using freq_table = std::array<uint64_t, constants::MAX_SIGMA>;

struct norm_freq {
    uint32_t org;
    uint32_t norm;
    uint8_t sym;
};

void normalize_freqs(
    freq_table& freqs, std::vector<uint16_t>& nfreqs, uint32_t frame_size)
{
    // (1) compute the counts
    uint64_t num_syms = 0;
    for (size_t i = 0; i < nfreqs.size(); i++) {
        num_syms += freqs[i];
    }

    // (2) crude normalization
    uint32_t actual_freq_csum = 0;
    std::vector<norm_freq> norm_freqs(nfreqs.size());
    for (size_t i = 0; i < nfreqs.size(); i++) {
        norm_freqs[i].sym = i;
        norm_freqs[i].org = freqs[i];
        norm_freqs[i].norm = (double(freqs[i]) / double(num_syms)) * frame_size;
        if (norm_freqs[i].norm == 0 && norm_freqs[i].org != 0)
            norm_freqs[i].norm = 1;
        actual_freq_csum += norm_freqs[i].norm;
    }

    // (3) fix things
    int32_t difference = int32_t(frame_size) - int32_t(actual_freq_csum);
    auto cmp_pdiff_func
        = [num_syms, frame_size](const norm_freq& a, const norm_freq& b) {
              double org_prob_a = double(a.org) / double(num_syms);
              double org_prob_b = double(b.org) / double(num_syms);
              double norm_prob_a = double(a.norm) / double(frame_size);
              if (a.norm == 1)
                  norm_prob_a = 0;
              double norm_prob_b = double(b.norm) / double(frame_size);
              if (b.norm == 1)
                  norm_prob_b = 0;
              return (norm_prob_b - org_prob_b) > (norm_prob_a - org_prob_a);
          };
    while (difference != 0) {
        std::sort(norm_freqs.begin(), norm_freqs.end(), cmp_pdiff_func);
        for (size_t i = 0; i < norm_freqs.size(); i++) {
            if (difference > 0) {
                norm_freqs[i].norm++;
                difference--;
                break;
            } else {
                if (norm_freqs[i].norm != 1) {
                    norm_freqs[i].norm--;
                    difference++;
                    break;
                }
            }
        }
    }

    // (4) put things back in order
    auto cmp_sym_func
        = [](const norm_freq& a, const norm_freq& b) { return a.sym < b.sym; };
    std::sort(norm_freqs.begin(), norm_freqs.end(), cmp_sym_func);

    // (5) check everything is ok
    actual_freq_csum = 0;
    for (size_t i = 0; i < nfreqs.size(); i++)
        actual_freq_csum += norm_freqs[i].norm;
    if (actual_freq_csum != frame_size) {
        quit("normalizing to framesize failed %u -> %u", frame_size,
            actual_freq_csum);
    }

    // (6) return actual normalized freqs
    for (size_t i = 0; i < norm_freqs.size(); i++) {
        nfreqs[i] = norm_freqs[i].norm;
    }
}
