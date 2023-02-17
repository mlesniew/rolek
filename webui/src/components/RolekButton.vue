<template>
  <div class="col-12 col-sm-6 col-md-4 p-1">
    <div :class="buttonGroupClass" role="group">
      <button
        type="button"
        :class="upDownButtonClass"
        @click="request('down')"
        :disabled="disabled"
      >
        &#9660;
      </button>
      <button
        type="button"
        :class="stopButtonClass"
        @click="request('stop')"
        :disabled="disabled"
      >
        {{ name || "&#8728;" }}
      </button>
      <button
        type="button"
        :class="upDownButtonClass"
        @click="request('up')"
        :disabled="disabled"
      >
        &#9650;
      </button>
    </div>
  </div>
</template>

<script>
import axios from "axios";

export default {
  props: {
    name: String,
    large: Boolean,
    disabled: Boolean,
  },

  emits: ["request_start", "request_success", "request_failure", "request_end"],

  methods: {
    request(direction) {
      let url = "/blinds/";
      if (this.name) {
        url += this.name + "/";
      }

      url += direction;

      this.$emit("request_start");
      axios
        .post(url)
        .then(() => {
          this.$emit("request_success");
        })
        .catch(() => {
          this.$emit("request_failure");
        })
        .then(() => {
          this.$emit("request_end");
        });
    },
  },

  data() {
    let upDownButtonClass = "btn btn-primary";

    let buttonGroupClass = "btn-group w-100 mb-sm-0 mb-3";
    if (this.large) {
      buttonGroupClass += " btn-group-lg";
    }

    let stopButtonClass = upDownButtonClass;
    if (this.name) {
      stopButtonClass += " w-100";
    }

    return {
      upDownButtonClass: upDownButtonClass,
      stopButtonClass: stopButtonClass,
      buttonGroupClass: buttonGroupClass,
      busy: true,
    };
  },
};
</script>
